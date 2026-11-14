/* Test-image-only #PF gate. Full GDT/IDT/exception ownership is Phase 4. */
#include <stddef.h>
#include <stdint.h>
#include "memory/init.h"
#include "memory/probe.h"
#include "arch/x86_64/io.h"
#include "arch/x86_64/serial.h"
#include "debug/log.h"
#include "debug/panic.h"

struct idt_gate {
    uint16_t low, selector;
    uint8_t ist, attributes;
    uint16_t middle;
    uint32_t high, reserved;
} __attribute__((packed));
struct idt_register { uint16_t limit; uint64_t base; } __attribute__((packed));
_Static_assert(sizeof(struct idt_gate) == 16, "x86_64 interrupt gate size");
static _Alignas(16) struct idt_gate idt[256];
static volatile uint64_t expected_address, expected_error, data_frame;
static volatile unsigned int selected_mode;
extern unsigned char __text_start[], __rodata_start[];
void memory_page_fault_entry(void);
void memory_execute_address(uint64_t address);
_Noreturn void memory_fault_report(uint64_t address, uint64_t error, uint64_t instruction);

static void install_probe_idt(void) {
    uint16_t selector;
    __asm__ volatile ("mov %%cs, %0" : "=r"(selector));
    uintptr_t handler = (uintptr_t)memory_page_fault_entry;
    idt[14].low = (uint16_t)handler;
    idt[14].selector = selector;
    idt[14].ist = 0;
    idt[14].attributes = 0x8e;
    idt[14].middle = (uint16_t)(handler >> 16);
    idt[14].high = (uint32_t)(handler >> 32);
    idt[14].reserved = 0;
    const struct idt_register descriptor = {
        .limit = (uint16_t)(sizeof(idt) - 1), .base = (uintptr_t)idt,
    };
    __asm__ volatile ("lidt %0" : : "m"(descriptor) : "memory");
}

_Noreturn void memory_fault_report(uint64_t address, uint64_t error, uint64_t instruction) {
    KASSERT(address == expected_address && error == expected_error);
    const char *kind;
    if (selected_mode == 5) { KASSERT(instruction == expected_address); kind = "nx"; }
    else {
        KASSERT(instruction >= (uintptr_t)__text_start && instruction < (uintptr_t)__rodata_start);
        kind = selected_mode == 4 ? "readonly" : "guard";
    }
    if (selected_mode == 4) {
        volatile uint64_t *original = memory_frame_pointer(data_frame);
        KASSERT(original != NULL && *original == UINT64_C(0x13579bdf2468ace0));
    }
    klog(LOG_INFO, "memory-probe", "%s fault: address=%p error=0x%llx ip=%p", kind,
         (void *)(uintptr_t)address, (unsigned long long)error, (void *)(uintptr_t)instruction);
    serial_write("AGENTICOS: PAGEFAULT PASS\n");
    cpu_halt();
}

_Noreturn void memory_fault_probe(unsigned int mode) {
    KASSERT(mode >= 4 && mode <= 6);
    selected_mode = mode;
    expected_address = mode == 6 ? MEMORY_HEAP_GUARD : MEMORY_SCRATCH;
    expected_error = mode == 4 ? UINT64_C(3) : mode == 5 ? UINT64_C(0x11) : 0;
    if (mode != 6) {
        uint64_t physical;
        KASSERT(pmm_alloc_frame(memory_frames(), &physical));
        data_frame = physical;
        volatile uint64_t *pointer = memory_frame_pointer(physical);
        KASSERT(pointer != NULL);
        *pointer = mode == 4 ? UINT64_C(0x13579bdf2468ace0) : UINT64_C(0xc3);
        KASSERT(vmm_map(memory_address_space(), MEMORY_SCRATCH, physical,
                       mode == 4 ? VM_NX : VM_WRITE | VM_NX));
    }
    install_probe_idt();
    if (mode == 4) { *(volatile uint64_t *)(uintptr_t)expected_address = 0; }
    else if (mode == 5) { memory_execute_address(expected_address); }
    else {
        volatile uint8_t value = *(volatile uint8_t *)(uintptr_t)expected_address;
        (void)value;
    }
    PANIC("memory protection probe returned without its expected page fault");
}
