#include "drivers.h"
#include "pci.h"
#include "input.h"
#include "ata.h"
#include "e1000.h"
#include "debug/panic.h"
#include "debug/log.h"
#include "arch/x86_64/serial.h"
#include "arch/x86_64/io.h"
#include "interrupts/interrupts.h"
#include "runtime/memory.h"
static bool disk, network;
void drivers_init(void) {
    size_t count = pci_scan(); KASSERT(count != 0);
    KASSERT(input_init()); disk = ata_init(); network = e1000_init();
    klog(LOG_INFO, "drivers", "PCI devices=%u; PS/2 keyboard/mouse polling ready; ATA=%u e1000=%u", (unsigned int)count, (unsigned int)disk, (unsigned int)network);
}
#if CONFIG_SELFTEST == 22
static void send_packet(uint8_t sequence) {
    unsigned char frame[64] = {0}; memset(frame, 0xff, 6); memcpy(frame + 6, e1000_mac(), 6);
    frame[12] = 0x88; frame[13] = 0xb5; frame[14] = sequence; memcpy(frame + 15, "AGENTICOS_DRIVER_TX", 19);
    KASSERT(e1000_send(frame, sizeof(frame)));
}
#endif
void drivers_self_test(void) {
#if CONFIG_SELFTEST == 22
    KASSERT(disk && network && ata_sectors() == 2048);
    unsigned char sector[512], expected[512];
    KASSERT(ata_read(0, sector) && memcmp(sector, "AgenticOS TEST DISK", 19) == 0);
    for (size_t i = 0; i < sizeof(expected); ++i) { expected[i] = (unsigned char)(i ^ 0x5au); }
    KASSERT(ata_write(1, expected) && ata_read(1, sector) && memcmp(sector, expected, 512) == 0);
    KASSERT(!ata_read(2048, sector) && !ata_write(2048, expected));
    serial_write("AGENTICOS: DRIVER STORAGE PASS\n");
    serial_write("AGENTICOS: DRIVER INPUT WAIT\n");
    bool keyboard = false, mouse = false; uint8_t sequence = 0;
    interrupts_timer_start(); uint64_t deadline = interrupts_ticks() + 1500;
    send_packet(sequence);
    while ((!keyboard || !mouse || sequence < 24) && interrupts_ticks() < deadline) {
        struct input_event event;
        while (input_next(&event)) {
            if (event.kind == INPUT_KEY && event.key == 'a') { keyboard = true; }
            if (event.kind == INPUT_MOUSE && event.dx != 0 && (event.buttons & 1u) != 0) { mouse = true; }
        }
        unsigned char frame[1518]; size_t length = e1000_receive(frame, sizeof(frame));
        if (length >= 34 && frame[12] == 0x88 && frame[13] == 0xb5 && frame[14] == sequence && memcmp(frame + 15, "AGENTICOS_DRIVER_RX", 19) == 0) {
            ++sequence; if (sequence < 24) { send_packet(sequence); }
        }
        __asm__ volatile ("sti; hlt; cli" : : : "memory");
    }
    interrupts_timer_stop();
    KASSERT(keyboard && mouse && sequence == 24 && input_dropped() == 0);
    serial_write("AGENTICOS: DRIVER INPUT PASS\n");
    serial_write("AGENTICOS: DRIVER NETWORK PASS\n");
#endif
    serial_write("AGENTICOS: PHASE8 PASS\n");
}
