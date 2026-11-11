#include "cpu/info.h"

struct cpuid_registers arch_cpuid(uint32_t leaf, uint32_t subleaf) {
    struct cpuid_registers result;
    __asm__ volatile ("cpuid" : "=a"(result.eax), "=b"(result.ebx),
                      "=c"(result.ecx), "=d"(result.edx) : "a"(leaf), "c"(subleaf));
    return result;
}
