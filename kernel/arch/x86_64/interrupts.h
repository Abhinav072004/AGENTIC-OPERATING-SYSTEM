#ifndef AGENTICOS_X86_INTERRUPTS_H
#define AGENTICOS_X86_INTERRUPTS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define X86_KERNEL_CODE 8u
#define X86_KERNEL_DATA 16u
#define X86_TSS_SELECTOR 24u
#define X86_IST_BYTES 16384u

/* Matches isr.asm, including the long-mode CPU's saved SS:RSP. */
struct interrupt_frame {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rdi, rsi, rbp, rdx, rcx, rbx, rax;
    uint64_t vector, error, rip, cs, rflags, rsp, ss;
};
struct x86_tss {
    uint32_t reserved0;
    uint64_t rsp[3], reserved1, ist[7], reserved2;
    uint16_t reserved3, iomap_base;
} __attribute__((packed));
struct x86_idt_gate {
    uint16_t low, selector;
    uint8_t ist, attributes;
    uint16_t middle;
    uint32_t high, reserved;
} __attribute__((packed));
struct x86_table_register { uint16_t limit; uint64_t base; } __attribute__((packed));
struct x86_interrupt_tables {
    uint64_t gdt[5];
    struct x86_tss tss;
    struct x86_idt_gate idt[256];
};

/* Pure descriptor construction, also executed by sanitized host tests. */
bool x86_interrupt_tables_build(struct x86_interrupt_tables *tables,
                               const uintptr_t handlers[256], uint64_t rsp0,
                               const uint64_t ist_tops[3]);
const char *x86_exception_name(uint64_t vector);
bool arch_interrupts_init(void);
unsigned int arch_interrupt_stack(uintptr_t pointer);
bool arch_interrupt_roundtrip(void);
void interrupt_dispatch(struct interrupt_frame *frame);

_Static_assert(sizeof(struct x86_tss) == 104, "64-bit TSS layout");
_Static_assert(sizeof(struct x86_idt_gate) == 16, "64-bit interrupt gate layout");
_Static_assert(sizeof(struct x86_table_register) == 10, "64-bit table register");
_Static_assert(offsetof(struct interrupt_frame, vector) == 120, "ISR vector offset");
_Static_assert(offsetof(struct interrupt_frame, rip) == 136, "ISR RIP offset");
_Static_assert(offsetof(struct interrupt_frame, rsp) == 160, "ISR RSP offset");
_Static_assert(sizeof(struct interrupt_frame) == 176, "ISR frame size");
#endif
