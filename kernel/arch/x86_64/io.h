#ifndef AGENTICOS_X86_IO_H
#define AGENTICOS_X86_IO_H

#include <stdint.h>

static inline uint64_t irq_save(void) {
    uint64_t flags;
    __asm__ volatile ("pushfq; pop %0; cli" : "=r"(flags) : : "memory");
    return flags;
}
static inline void irq_restore(uint64_t flags) {
    /* Restore the caller's IF without reintroducing a saved DF into C code. */
    if ((flags & UINT64_C(0x200)) != 0) { __asm__ volatile ("sti" : : : "memory"); }
    else { __asm__ volatile ("cli" : : : "memory"); }
}

static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}
static inline void outl(uint16_t port, uint32_t value) { __asm__ volatile ("outl %0, %1" : : "a"(value), "Nd"(port)); }
static inline uint32_t inl(uint16_t port) { uint32_t value; __asm__ volatile ("inl %1, %0" : "=a"(value) : "Nd"(port)); return value; }
static inline void outw(uint16_t port, uint16_t value) { __asm__ volatile ("outw %0, %1" : : "a"(value), "Nd"(port)); }
static inline uint16_t inw(uint16_t port) { uint16_t value; __asm__ volatile ("inw %1, %0" : "=a"(value) : "Nd"(port)); return value; }

static inline _Noreturn void cpu_halt(void) {
    for (;;) {
        __asm__ volatile ("cli; hlt" : : : "memory");
    }
}

#endif
