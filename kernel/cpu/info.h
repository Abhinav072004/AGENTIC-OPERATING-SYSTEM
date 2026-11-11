#ifndef AGENTICOS_CPU_INFO_H
#define AGENTICOS_CPU_INFO_H

#include <stdbool.h>
#include <stdint.h>

struct cpuid_registers { uint32_t eax, ebx, ecx, edx; };
typedef struct cpuid_registers (*cpuid_reader)(uint32_t leaf, uint32_t subleaf);

struct cpu_info {
    char vendor[13];
    char brand[49];
    uint32_t family, model, stepping;
    bool apic, sse2, nx, long_mode, hypervisor;
};

/* Caller owns the result. Only supported leaves are read; no allocation. */
void cpu_info_read(struct cpu_info *info, cpuid_reader reader);
struct cpuid_registers arch_cpuid(uint32_t leaf, uint32_t subleaf);

#endif
