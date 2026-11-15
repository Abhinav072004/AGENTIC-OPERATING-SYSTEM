#include "interrupts.h"
#include "runtime/memory.h"

static bool canonical(uint64_t value) {
    return value <= UINT64_C(0x00007fffffffffff) || value >= UINT64_C(0xffff800000000000);
}

bool x86_interrupt_tables_build(struct x86_interrupt_tables *tables,
                               const uintptr_t handlers[256], uint64_t rsp0,
                               const uint64_t ist_tops[3]) {
    if (tables == NULL || handlers == NULL || ist_tops == NULL || rsp0 == 0 ||
        !canonical(rsp0) || rsp0 % 16 != 0) { return false; }
    for (size_t i = 0; i < 256; ++i) {
        if (handlers[i] == 0 || !canonical(handlers[i])) { return false; }
    }
    for (size_t i = 0; i < 3; ++i) {
        if (ist_tops[i] < X86_IST_BYTES || !canonical(ist_tops[i]) ||
            !canonical(ist_tops[i] - X86_IST_BYTES) || ist_tops[i] % 16 != 0) { return false; }
    }
    memset(tables, 0, sizeof(*tables));
    tables->gdt[1] = UINT64_C(0x00af9a000000ffff); /* Present ring-0 long-mode code. */
    tables->gdt[2] = UINT64_C(0x00cf92000000ffff); /* Present ring-0 writable data. */
    tables->tss.rsp[0] = rsp0;
    tables->tss.iomap_base = sizeof(struct x86_tss); /* Beyond limit: no I/O bitmap. */
    for (size_t i = 0; i < 3; ++i) { tables->tss.ist[i] = ist_tops[i]; }
    uint64_t base = (uintptr_t)&tables->tss;
    uint64_t limit = sizeof(struct x86_tss) - 1;
    tables->gdt[3] = limit | ((base & UINT64_C(0xffffff)) << 16) |
                     (UINT64_C(0x89) << 40) | ((base & UINT64_C(0xff000000)) << 32);
    tables->gdt[4] = base >> 32;
    for (size_t i = 0; i < 256; ++i) {
        uintptr_t handler = handlers[i];
        struct x86_idt_gate *gate = &tables->idt[i];
        gate->low = (uint16_t)handler;
        gate->middle = (uint16_t)(handler >> 16);
        gate->high = (uint32_t)(handler >> 32);
        gate->selector = X86_KERNEL_CODE;
        gate->attributes = 0x8e; /* Present interrupt gate, DPL 0; clears live IF. */
        gate->ist = (uint8_t)(i == 8 ? 1 : i == 2 ? 2 : i == 18 ? 3 : 0);
    }
    return true;
}

const char *x86_exception_name(uint64_t vector) {
    static const char *const names[32] = {
        "divide error", "debug", "non-maskable interrupt", "breakpoint",
        "overflow", "bound range", "invalid opcode", "device unavailable",
        "double fault", "reserved", "invalid TSS", "segment not present",
        "stack segment", "general protection", "page fault", "reserved",
        "x87 floating point", "alignment check", "machine check", "SIMD floating point",
        "virtualization", "control protection", "reserved", "reserved",
        "reserved", "reserved", "reserved", "reserved", "hypervisor injection",
        "VMM communication", "security", "reserved",
    };
    return vector < 32 ? names[vector] : "unexpected interrupt";
}
