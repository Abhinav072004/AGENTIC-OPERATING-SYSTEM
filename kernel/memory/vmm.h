#ifndef AGENTICOS_VMM_H
#define AGENTICOS_VMM_H

#include <stdbool.h>
#include <stdint.h>

#define VM_WRITE UINT64_C(0x002)
#define VM_WRITE_THROUGH UINT64_C(0x008)
#define VM_CACHE_DISABLE UINT64_C(0x010)
#define VM_NX (UINT64_C(1) << 63)

/* Backends own table frames. pointer() returns a stable, aligned 4096-byte
 * table or NULL; free_table() must succeed for owned frames. Call create once
 * per fresh space and destroy before reuse. Active state is managed by the
 * architecture owner; no concurrent edits, SMP shootdown or shared tables. */
struct vmm_ops {
    void *context;
    bool (*alloc_table)(void *context, uint64_t *physical);
    void (*free_table)(void *context, uint64_t physical);
    void *(*pointer)(void *context, uint64_t physical);
    void (*invalidate)(void *context, uint64_t address);
};
struct vmm {
    struct vmm_ops ops;
    uint64_t root, table_pages, mapped_pages;
    bool ready, active;
};

bool vmm_create(struct vmm *space, const struct vmm_ops *ops);
/* Four-level supervisor-only 4 KiB mappings. Caller owns data frames. */
bool vmm_map(struct vmm *space, uint64_t address, uint64_t physical, uint64_t flags);
bool vmm_query(const struct vmm *space, uint64_t address, uint64_t *physical, uint64_t *flags);
/* Changes permissions, not cache policy. Callers keep cache types consistent
 * across physical aliases; changing PAT/cache modes requires separate support. */
bool vmm_protect(struct vmm *space, uint64_t address, uint64_t flags);
/* Frees empty tables, never the caller's data frame. */
bool vmm_unmap(struct vmm *space, uint64_t address, uint64_t *physical);
bool vmm_destroy(struct vmm *space);

#endif
