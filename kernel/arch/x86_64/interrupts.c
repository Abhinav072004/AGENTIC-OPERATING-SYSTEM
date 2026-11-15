#include "interrupts.h"
#include "debug/log.h"

static _Alignas(16) struct x86_interrupt_tables tables;
static _Alignas(16) unsigned char ist_stacks[3][X86_IST_BYTES];
extern unsigned char stack_top[];
extern const uintptr_t interrupt_stub_table[256];
void arch_load_gdt(const struct x86_table_register *descriptor);

bool arch_interrupts_init(void) {
    uint64_t tops[3];
    for (size_t i = 0; i < 3; ++i) { tops[i] = (uintptr_t)&ist_stacks[i][X86_IST_BYTES]; }
    if (!x86_interrupt_tables_build(&tables, interrupt_stub_table, (uintptr_t)stack_top, tops)) { return false; }
    const struct x86_table_register gdtr = {sizeof(tables.gdt) - 1, (uintptr_t)tables.gdt};
    const struct x86_table_register idtr = {sizeof(tables.idt) - 1, (uintptr_t)tables.idt};
    arch_load_gdt(&gdtr);
    __asm__ volatile ("lidt %0" : : "m"(idtr) : "memory");
    struct x86_table_register actual_gdt, actual_idt;
    uint16_t tr, cs, ss;
    __asm__ volatile ("sgdt %0; sidt %1; str %2; mov %%cs, %3; mov %%ss, %4"
                      : "=m"(actual_gdt), "=m"(actual_idt), "=r"(tr), "=r"(cs), "=r"(ss));
    uint64_t loaded_tss = *(volatile uint64_t *)&tables.gdt[3];
    if (actual_gdt.base != gdtr.base || actual_gdt.limit != gdtr.limit ||
        actual_idt.base != idtr.base || actual_idt.limit != idtr.limit ||
        tr != X86_TSS_SELECTOR || cs != X86_KERNEL_CODE || ss != X86_KERNEL_DATA ||
        ((loaded_tss >> 40) & UINT64_C(0xff)) != UINT64_C(0x8b)) { return false; }
    klog(LOG_INFO, "interrupt", "GDT=%p limit=%u IDT=%p limit=%u CS=0x%x SS=0x%x TR=0x%x TSS_busy=1",
         (void *)(uintptr_t)actual_gdt.base, (unsigned int)actual_gdt.limit,
         (void *)(uintptr_t)actual_idt.base, (unsigned int)actual_idt.limit,
         (unsigned int)cs, (unsigned int)ss, (unsigned int)tr);
    klog(LOG_INFO, "interrupt", "256 gates; dedicated IST: double-fault=1 NMI=2 machine-check=3");
    return true;
}

unsigned int arch_interrupt_stack(uintptr_t pointer) {
    for (size_t i = 0; i < 3; ++i) {
        uintptr_t bottom = (uintptr_t)ist_stacks[i];
        if (pointer >= bottom && pointer < bottom + X86_IST_BYTES) { return (unsigned int)i + 1; }
    }
    return 0;
}
