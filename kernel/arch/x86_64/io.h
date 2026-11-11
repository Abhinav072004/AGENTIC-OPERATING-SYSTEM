#ifndef AGENTICOS_X86_IO_H
#define AGENTICOS_X86_IO_H

#include <stdint.h>

static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline _Noreturn void cpu_halt(void) {
    for (;;) {
        __asm__ volatile ("cli; hlt" : : : "memory");
    }
}

#endif
