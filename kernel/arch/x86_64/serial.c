#include "serial.h"
#include "io.h"

enum { COM1 = 0x3f8, POLL_LIMIT = 100000 };

void serial_init(void) {
    outb(COM1 + 1, 0x00); /* Polling only, no UART interrupts. */
    outb(COM1 + 3, 0x80); /* Divisor latch. */
    outb(COM1,     0x01); /* 115200 baud. */
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03); /* Eight data bits, no parity, one stop bit. */
    outb(COM1 + 2, 0xc7); /* Enable and clear FIFOs. */
    outb(COM1 + 4, 0x0b); /* DTR, RTS, OUT2. */
}

static void serial_putc(char value) {
    /* A missing/broken UART must not hang boot indefinitely. */
    for (unsigned int i = 0; i < POLL_LIMIT; ++i) {
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
