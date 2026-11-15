#ifndef AGENTICOS_MEMORY_PROBE_H
#define AGENTICOS_MEMORY_PROBE_H
#include <stdint.h>
/* Linked only into memory-ro/nx/guard test images; uses the owned ISR path. */
_Noreturn void memory_fault_probe(unsigned int mode);
_Noreturn void memory_fault_report(uint64_t address, uint64_t error, uint64_t instruction);
#endif
