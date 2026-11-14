#ifndef AGENTICOS_MEMORY_INIT_H
#define AGENTICOS_MEMORY_INIT_H
#include "pmm.h"
#include "vmm.h"
#include "heap.h"

#define MEMORY_HHDM_BASE UINT64_C(0xffff800000000000)
#define MEMORY_DIRECT_LIMIT (UINT64_C(1) << 46)
#define MEMORY_HEAP_GUARD UINT64_C(0xffffd00000000000)
#define MEMORY_HEAP_START (MEMORY_HEAP_GUARD + PAGE_SIZE)
#define MEMORY_HEAP_BYTES ((size_t)262144)
#define MEMORY_SCRATCH UINT64_C(0xffffd10000000000)
#define MEMORY_SKIP_REGION UINT64_MAX

/* Owned copy of the boot map. Non-usable RAM stays mapped but not allocatable. */
struct memory_boot {
    struct memory_region regions[PMM_REGION_LIMIT];
    uint64_t direct_flags[PMM_REGION_LIMIT];
    size_t count;
    uint64_t hhdm, kernel_physical, kernel_virtual;
};
bool memory_bootstrap(const struct memory_boot *boot);
void memory_self_test(void);
void memory_exhaustion_test(void);
void *kmalloc(size_t bytes);
bool kfree(void *pointer);
struct pmm *memory_frames(void);
struct vmm *memory_address_space(void);
void *memory_frame_pointer(uint64_t physical);
#endif
