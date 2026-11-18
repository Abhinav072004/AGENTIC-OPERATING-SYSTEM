#include "e1000.h"
#include "pci.h"
#include "memory/init.h"
#include "runtime/memory.h"
#include <stdatomic.h>
#define MMIO UINT64_C(0xffffd30000000000)
#define RING_SIZE 16u
struct rx_descriptor { uint64_t address; uint16_t length, checksum; uint8_t status, errors; uint16_t special; };
struct tx_descriptor { uint64_t address; uint16_t length; uint8_t cso, command, status, css; uint16_t special; };
_Static_assert(sizeof(struct rx_descriptor) == 16 && sizeof(struct tx_descriptor) == 16, "hardware descriptors");
static volatile uint32_t *registers;
static volatile struct rx_descriptor *rx;
static volatile struct tx_descriptor *tx;
static uint64_t frames[2 + 2 * RING_SIZE];
static uint8_t mac[6];
static unsigned int rx_next, tx_next;
static bool ready;
static uint32_t read_register(size_t offset) { return registers[offset / 4u]; }
static void write_register(size_t offset, uint32_t value) { registers[offset / 4u] = value; (void)read_register(8); }
bool e1000_ready(void) { return ready; }
const uint8_t *e1000_mac(void) { return mac; }
bool e1000_init(void) {
    if (ready) { return true; }
    struct pci_device device;
    if (!pci_find(0x8086, 0x100e, &device)) { return false; }
    uint32_t bar = pci_read(&device, 0x10);
    if ((bar & 15u) != 0 || bar == 0) { return false; } /* Tested 32-bit memory BAR only. */
    uint64_t physical = bar & UINT32_C(0xfffffff0);
    size_t mapped = 0, owned = 0;
    for (; mapped < 32; ++mapped) {
        if (!vmm_map(memory_address_space(), MMIO + mapped * PAGE_SIZE, physical + mapped * PAGE_SIZE,
                     VM_WRITE | VM_NX | VM_WRITE_THROUGH | VM_CACHE_DISABLE)) { goto rollback; }
    }
    for (; owned < 2 + 2 * RING_SIZE; ++owned) {
        if (!pmm_alloc_frame(memory_frames(), &frames[owned])) { goto rollback; }
        memset(memory_frame_pointer(frames[owned]), 0, PAGE_SIZE);
    }
    /* Do not write back PCI status's write-one-to-clear bits. */
    pci_write(&device, 4, (pci_read(&device, 4) & 0xffffu) | 6u);
    registers = (volatile uint32_t *)(uintptr_t)MMIO;
    write_register(0xd8, UINT32_MAX); /* IMC: interrupts remain disabled. */
    write_register(0, read_register(0) | (UINT32_C(1) << 26));
    for (unsigned int i = 0; (read_register(0) & (UINT32_C(1) << 26)) != 0; ++i) { if (i == 1000000) { goto disable; } }
    write_register(0xd8, UINT32_MAX); (void)read_register(0xc0);
    write_register(0, read_register(0) | 0x40u); /* Set link up. */
    uint32_t low = read_register(0x5400), high = read_register(0x5404);
    for (unsigned int i = 0; i < 4; ++i) { mac[i] = (uint8_t)(low >> (8u * i)); }
    mac[4] = (uint8_t)high; mac[5] = (uint8_t)(high >> 8);
    write_register(0x5404, high | UINT32_C(0x80000000));
    for (size_t i = 0; i < 128; ++i) { write_register(0x5200 + i * 4u, 0); }
    rx = memory_frame_pointer(frames[0]); tx = memory_frame_pointer(frames[1]);
    for (size_t i = 0; i < RING_SIZE; ++i) { rx[i].address = frames[2 + i]; tx[i].address = frames[2 + RING_SIZE + i]; tx[i].status = 1; }
    atomic_thread_fence(memory_order_seq_cst);
    write_register(0x2800, (uint32_t)frames[0]); write_register(0x2804, (uint32_t)(frames[0] >> 32));
    write_register(0x2808, RING_SIZE * 16u); write_register(0x2810, 0); write_register(0x2818, RING_SIZE - 1u);
    write_register(0x3800, (uint32_t)frames[1]); write_register(0x3804, (uint32_t)(frames[1] >> 32));
    write_register(0x3808, RING_SIZE * 16u); write_register(0x3810, 0); write_register(0x3818, 0);
    write_register(0x410, 0x60200a); write_register(0x400, 2u | 8u | (0x10u << 4) | (0x40u << 12));
    write_register(0x100, 2u | (1u << 15) | (1u << 26));
    rx_next = tx_next = 0; ready = true; return true;
disable:
    write_register(0x100, 0); write_register(0x400, 0); pci_write(&device, 4, pci_read(&device, 4) & 0xfffbu);
rollback:
    while (owned != 0) { --owned; (void)pmm_free_frame(memory_frames(), frames[owned]); }
    while (mapped != 0) { --mapped; (void)vmm_unmap(memory_address_space(), MMIO + mapped * PAGE_SIZE, NULL); }
    registers = NULL; rx = NULL; tx = NULL; return false;
}
bool e1000_send(const void *frame, size_t length) {
    if (!ready || frame == NULL || length < 14 || length > 1518 || (tx[tx_next].status & 1u) == 0) { return false; }
    unsigned int index = tx_next;
    memcpy(memory_frame_pointer(frames[2 + RING_SIZE + index]), frame, length);
    tx[index].length = (uint16_t)length; tx[index].command = 0x0b; tx[index].status = 0;
    atomic_thread_fence(memory_order_seq_cst);
    tx_next = (index + 1u) % RING_SIZE; write_register(0x3818, tx_next);
    for (unsigned int i = 0; i < 1000000; ++i) { if ((tx[index].status & 1u) != 0) { return true; } }
    return false;
}
size_t e1000_receive(void *frame, size_t capacity) {
    if (!ready || frame == NULL || (rx[rx_next].status & 1u) == 0) { return 0; }
    atomic_thread_fence(memory_order_acquire);
    unsigned int index = rx_next; size_t length = rx[index].length;
    bool valid = (rx[index].status & 2u) != 0 && rx[index].errors == 0 && length >= 14 && length <= 1518 && length <= capacity;
    if (valid) { memcpy(frame, memory_frame_pointer(frames[2 + index]), length); }
    rx[index].status = 0;
    atomic_thread_fence(memory_order_seq_cst);
    write_register(0x2818, index); rx_next = (index + 1u) % RING_SIZE;
    return valid ? length : 0;
}
