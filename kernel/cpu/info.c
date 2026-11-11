#include "info.h"
#include <stddef.h>

static void unpack(char *destination, uint32_t value) {
    for (unsigned int i = 0; i < 4; ++i) { destination[i] = (char)(value >> (i * 8)); }
}

void cpu_info_read(struct cpu_info *info, cpuid_reader reader) {
    struct cpuid_registers base = reader(0, 0);
    unpack(info->vendor, base.ebx);
    unpack(info->vendor + 4, base.edx);
    unpack(info->vendor + 8, base.ecx);
    info->vendor[12] = '\0';
    info->brand[0] = '\0';
    info->family = 0; info->model = 0; info->stepping = 0;
    info->apic = false; info->sse2 = false; info->nx = false;
    info->long_mode = false; info->hypervisor = false;
    if (base.eax >= 1) {
        struct cpuid_registers features = reader(1, 0);
        uint32_t family = (features.eax >> 8) & 15;
        info->family = family;
        info->model = (features.eax >> 4) & 15;
        if (family == 15) { info->family += (features.eax >> 20) & 255; }
        if (family == 6 || family == 15) { info->model |= ((features.eax >> 16) & 15) << 4; }
        info->stepping = features.eax & 15;
        info->apic = (features.edx & (UINT32_C(1) << 9)) != 0;
        info->sse2 = (features.edx & (UINT32_C(1) << 26)) != 0;
        info->hypervisor = (features.ecx & (UINT32_C(1) << 31)) != 0;
    }
    uint32_t maximum = reader(UINT32_C(0x80000000), 0).eax;
    if (maximum >= UINT32_C(0x80000001)) {
        struct cpuid_registers extended = reader(UINT32_C(0x80000001), 0);
        info->nx = (extended.edx & (UINT32_C(1) << 20)) != 0;
        info->long_mode = (extended.edx & (UINT32_C(1) << 29)) != 0;
    }
    if (maximum >= UINT32_C(0x80000004)) {
        for (uint32_t i = 0; i < 3; ++i) {
            struct cpuid_registers brand = reader(UINT32_C(0x80000002) + i, 0);
            unpack(info->brand + i * 16, brand.eax);
            unpack(info->brand + i * 16 + 4, brand.ebx);
            unpack(info->brand + i * 16 + 8, brand.ecx);
            unpack(info->brand + i * 16 + 12, brand.edx);
        }
        info->brand[48] = '\0';
    }
}
