#ifndef AGENTICOS_ARCH_MEMORY_H
#define AGENTICOS_ARCH_MEMORY_H
#include <stdbool.h>
#include <stdint.h>
bool arch_memory_activate(uint64_t root);
void arch_memory_invalidate(uint64_t address);
uint64_t arch_memory_root(void);
bool arch_memory_protection_enabled(void);
#endif
