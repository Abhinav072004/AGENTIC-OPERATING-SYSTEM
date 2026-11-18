#include "serial.h"
#include "io.h"

enum { COM1 = 0x3f8, POLL_LIMIT = 100000, TX_POLL_LIMIT = 1000000 };

bool serial_try_read(char *character) {
    if (character == 0) { return false; }
    uint8_t status = inb(COM1 + 5);
    if ((status & 1) == 0) { return false; }
    uint8_t value = inb(COM1);
    if ((status & 0x1e) != 0) { return false; }
    *character = (char)value;
    return true;
}

bool serial_init(void) {
    outb(COM1 + 1, 0x00); /* Polling only, no UART interrupts. */
    outb(COM1 + 3, 0x80); /* Divisor latch. */
    outb(COM1,     0x01); /* 115200 baud. */
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03); /* Eight data bits, no parity, one stop bit. */
    outb(COM1 + 2, 0xc7); /* Enable and clear FIFOs. */
    outb(COM1 + 4, 0x1e); /* Internal loopback: verify both transmit and receive. */
    outb(COM1, 0xae);
    bool ready = false;
    for (unsigned int i = 0; i < POLL_LIMIT; ++i) {
        char received;
        if (serial_try_read(&received)) {
            ready = (uint8_t)received == 0xae;
            break;
        }
    }
    outb(COM1 + 4, 0x0b); /* Normal mode: DTR, RTS, OUT2. */
    return ready;
}

static void serial_putc(char value) {
    /* A missing/broken UART must not hang boot indefinitely. */
    for (unsigned int i = 0; i < TX_POLL_LIMIT; ++i) {
        if ((inb(COM1 + 5) & 0x20) != 0) {
            outb(COM1, (uint8_t)value);
            return;
        }
    }
}

void serial_write(const char *text) {
    while (*text != '\0') {
        if (*text == '\n') {
            serial_putc('\r');
        }
        serial_putc(*text++);
    }
}
