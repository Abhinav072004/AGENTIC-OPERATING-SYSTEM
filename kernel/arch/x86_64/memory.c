#include "memory.h"
#include "cpu/info.h"

static uint64_t efer_read(void) {
    uint32_t low, high;
    __asm__ volatile ("rdmsr" : "=a"(low), "=d"(high) : "c"(UINT32_C(0xc0000080)));
    return ((uint64_t)high << 32) | low;
}
uint64_t arch_memory_root(void) {
    uint64_t root;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(root));
    return root & UINT64_C(0x000ffffffffff000);
}
bool arch_memory_protection_enabled(void) {
    uint64_t cr0;
    __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));
    return (cr0 & (UINT64_C(1) << 16)) != 0 && (efer_read() & (UINT64_C(1) << 11)) != 0;
}
bool arch_memory_activate(uint64_t root) {
    uint64_t cr0, cr4;
    __asm__ volatile ("mov %%cr4, %0" : "=r"(cr4));
    if ((root & ~UINT64_C(0x000ffffffffff000)) != 0 ||
        (cr4 & (UINT64_C(1) << 12)) != 0 ||
        arch_cpuid(UINT32_C(0x80000000), 0).eax < UINT32_C(0x80000001) ||
        (arch_cpuid(UINT32_C(0x80000001), 0).edx & (UINT32_C(1) << 20)) == 0) { return false; }
    uint64_t efer = efer_read() | (UINT64_C(1) << 11);
    __asm__ volatile ("wrmsr" : : "c"(UINT32_C(0xc0000080)),
                      "a"((uint32_t)efer), "d"((uint32_t)(efer >> 32)) : "memory");
    __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= UINT64_C(1) << 16;
    __asm__ volatile ("mov %0, %%cr0" : : "r"(cr0) : "memory");
    /* CR3 alone does not flush inherited global translations when PGE is set. */
    uint64_t without_global = cr4 & ~(UINT64_C(1) << 7);
    __asm__ volatile ("mov %0, %%cr4" : : "r"(without_global) : "memory");
    __asm__ volatile ("mov %0, %%cr3" : : "r"(root) : "memory");
    __asm__ volatile ("mov %0, %%cr4" : : "r"(cr4) : "memory");
    return arch_memory_root() == root && arch_memory_protection_enabled();
}
void arch_memory_invalidate(uint64_t address) {
    __asm__ volatile ("invlpg (%0)" : : "r"((uintptr_t)address) : "memory");
}
bool arch_memory_switch(uint64_t root) {
    if ((root & ~UINT64_C(0x000ffffffffff000)) != 0) { return false; }
    if (arch_memory_root() != root) { __asm__ volatile ("mov %0, %%cr3" : : "r"(root) : "memory"); }
    return arch_memory_root() == root;
}
