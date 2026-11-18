#include "input.h"
#include "arch/x86_64/io.h"
bool input_pop(struct input_event *event);
static bool send(uint16_t port, uint8_t byte) { for (unsigned int i = 0; i < 100000; ++i) { if ((inb(0x64) & 2u) == 0) { outb(port, byte); return true; } } return false; }
static bool receive(uint8_t *byte) { for (unsigned int i = 0; i < 100000; ++i) { if ((inb(0x64) & 1u) != 0) { *byte = inb(0x60); return true; } } return false; }
static bool device_command(bool mouse, uint8_t command) { uint8_t ack; return (!mouse || send(0x64, 0xd4)) && send(0x60, command) && receive(&ack) && ack == 0xfa; }
bool input_init(void) {
    input_reset();
    if (!send(0x64, 0xad) || !send(0x64, 0xa7)) { return false; }
    for (unsigned int i = 0; i < 64 && (inb(0x64) & 1u) != 0; ++i) { (void)inb(0x60); }
    uint8_t config;
    if (!send(0x64, 0x20) || !receive(&config) || !send(0x64, 0x60) || !send(0x60, (uint8_t)((config | 0x40u) & ~0x33u)) || !send(0x64, 0xae) || !send(0x64, 0xa8)) { return false; }
    return device_command(false, 0xf4) && device_command(true, 0xf6) && device_command(true, 0xf4);
}
bool input_next(struct input_event *event) {
    /* Consume queued events before polling another hardware batch. */
    if (input_pop(event)) { return true; }
    for (unsigned int i = 0; i < 64; ++i) {
        uint8_t status = inb(0x64); if ((status & 1u) == 0) { break; }
        uint8_t byte = inb(0x60); if ((status & 0xc0u) != 0) { continue; }
        input_decode(byte, (status & 0x20u) != 0);
    }
    return input_pop(event);
}
