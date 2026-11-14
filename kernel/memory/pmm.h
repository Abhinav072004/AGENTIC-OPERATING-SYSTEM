#ifndef AGENTICOS_PMM_H
#define AGENTICOS_PMM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PAGE_SIZE UINT64_C(4096)
#define PMM_REGION_LIMIT 128u

struct memory_region { uint64_t base, length; bool usable; };

/* Caller retains bitmap storage for the allocator's lifetime. Only eligible
 * frames may be allocated. Used bits track live allocations, not reservations.
 * Bootstrap CPU only: callers serialize all operations. Reinit requires no
 * live allocations or users of the old allocator. */
struct pmm {
    uint8_t *eligible, *used;
    uint64_t frame_count, managed_frames, free_frames, cursor;
    bool ready;
};

bool pmm_storage_size(const struct memory_region *regions, size_t count, size_t *bytes);
bool pmm_init(struct pmm *pmm, const struct memory_region *regions, size_t count,
              void *storage, size_t bytes);
/* Whole free usable frames only; rejection leaves the allocator unchanged. */
bool pmm_reserve(struct pmm *pmm, uint64_t physical, uint64_t bytes);
/* Boolean result permits physical address zero to be a valid allocation. */
bool pmm_alloc_frame(struct pmm *pmm, uint64_t *physical);
bool pmm_free_frame(struct pmm *pmm, uint64_t physical);
bool pmm_is_allocated(const struct pmm *pmm, uint64_t physical);

#endif
