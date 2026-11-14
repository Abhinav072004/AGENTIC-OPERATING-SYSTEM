#ifndef AGENTICOS_MEMORY_PROBE_H
#define AGENTICOS_MEMORY_PROBE_H
/* Linked only into memory-ro/nx/guard test images. No production IDT here. */
_Noreturn void memory_fault_probe(unsigned int mode);
#endif
