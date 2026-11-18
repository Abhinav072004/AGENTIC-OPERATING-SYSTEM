#include "memory/vmm.h"
#include <stddef.h>

#define PRESENT UINT64_C(1)
#define LARGE UINT64_C(0x80)
#define ACCESSED_DIRTY UINT64_C(0x60)
#define ADDRESS_MASK UINT64_C(0x000ffffffffff000)
#define ALLOWED (VM_WRITE | VM_USER | VM_WRITE_THROUGH | VM_CACHE_DISABLE | VM_NX)

static bool canonical(uint64_t address) {
    return address >> 47 == 0 || address >> 47 == UINT64_C(0x1ffff);
}
static bool valid_page(uint64_t address) { return canonical(address) && address % 4096 == 0; }
static bool valid_flags(uint64_t flags) {
    return (flags & ~ALLOWED) == 0 && ((flags & VM_WRITE) == 0 || (flags & VM_NX) != 0);
}
static volatile uint64_t *table(const struct vmm *space, uint64_t physical) {
    return space->ops.pointer(space->ops.context, physical);
}
static bool new_table(struct vmm *space, uint64_t *physical) {
    if (!space->ops.alloc_table(space->ops.context, physical)) { return false; }
    volatile uint64_t *entries = table(space, *physical);
    if (entries == NULL || (uintptr_t)entries % 4096 != 0 ||
        (*physical & ~ADDRESS_MASK) != 0) {
        space->ops.free_table(space->ops.context, *physical);
        return false;
    }
    for (size_t i = 0; i < 512; ++i) { entries[i] = 0; }
    ++space->table_pages;
    return true;
}
static void release_table(struct vmm *space, uint64_t physical) {
    space->ops.free_table(space->ops.context, physical);
    --space->table_pages;
}

bool vmm_create(struct vmm *space, const struct vmm_ops *ops) {
    if (space == NULL || ops == NULL || ops->alloc_table == NULL || ops->free_table == NULL ||
        ops->pointer == NULL || ops->invalidate == NULL) { return false; }
    space->ready = false;
    space->active = false;
    space->shared_upper = false;
    space->table_pages = 0;
    space->mapped_pages = 0;
    space->ops = *ops;
    if (!new_table(space, &space->root)) { return false; }
    space->ready = true;
    return true;
}

bool vmm_create_user(struct vmm *space, const struct vmm *kernel) {
    if (space == NULL || kernel == NULL || space == kernel || !kernel->ready || kernel->shared_upper ||
        !vmm_create(space, &kernel->ops)) { return false; }
    volatile uint64_t *source = table(kernel, kernel->root);
    volatile uint64_t *destination = table(space, space->root);
    if (source == NULL || destination == NULL) { release_table(space, space->root); space->ready = false; return false; }
    for (size_t i = 256; i < 512; ++i) {
        if ((source[i] & VM_USER) != 0) { release_table(space, space->root); space->ready = false; return false; }
        destination[i] = source[i];
    }
    space->shared_upper = true;
    return true;
}
static volatile uint64_t *leaf(const struct vmm *space, uint64_t address, uint64_t *permissions) {
    if (space == NULL || !space->ready || !canonical(address)) { return NULL; }
    volatile uint64_t *entries = table(space, space->root);
    uint64_t allowed = VM_WRITE | VM_USER, nx = 0;
    for (unsigned int shift = 39; shift > 12; shift -= 9) {
        if (entries == NULL) { return NULL; }
        uint64_t entry = entries[(address >> shift) & 511];
        if ((entry & PRESENT) == 0 || (entry & LARGE) != 0) { return NULL; }
        allowed &= entry;
        nx |= entry & VM_NX;
        entries = table(space, entry & ADDRESS_MASK);
    }
    if (entries == NULL) { return NULL; }
    if (permissions != NULL) { *permissions = (allowed & (VM_WRITE | VM_USER)) | nx; }
    return &entries[(address >> 12) & 511];
}

bool vmm_map(struct vmm *space, uint64_t address, uint64_t physical, uint64_t flags) {
    if (space == NULL || !space->ready || address == 0 || !valid_page(address) ||
        (physical & ~ADDRESS_MASK) != 0 || !valid_flags(flags) ||
        (space->shared_upper && address >= UINT64_C(0xffff800000000000)) ||
        ((flags & VM_USER) != 0 && (!space->shared_upper || address >= UINT64_C(0x800000000000)))) { return false; }
    volatile uint64_t *entries = table(space, space->root);
    volatile uint64_t *created_links[3];
    uint64_t created_frames[3];
    size_t created = 0;
    for (unsigned int shift = 39; shift > 12; shift -= 9) {
        if (entries == NULL) { goto rollback; }
        volatile uint64_t *slot = &entries[(address >> shift) & 511];
        uint64_t entry = *slot;
        if ((entry & PRESENT) == 0) {
            uint64_t child;
            if (!new_table(space, &child)) { goto rollback; }
            created_links[created] = slot;
            created_frames[created++] = child;
            *slot = child | PRESENT | VM_WRITE | (flags & VM_USER);
            entry = *slot;
        } else if ((entry & LARGE) != 0) { goto rollback; }
        if ((flags & VM_USER) != 0 && (entry & VM_USER) == 0) { goto rollback; }
        entries = table(space, entry & ADDRESS_MASK);
    }
    if (entries == NULL) { goto rollback; }
    volatile uint64_t *slot = &entries[(address >> 12) & 511];
    if ((*slot & PRESENT) != 0) { goto rollback; }
    *slot = physical | flags | PRESENT;
    ++space->mapped_pages;
    if (space->active) { space->ops.invalidate(space->ops.context, address); }
    return true;
rollback:
    /* No leaf installed: undo only newly allocated path nodes, deepest first. */
    while (created != 0) {
        --created;
        *created_links[created] = 0;
        release_table(space, created_frames[created]);
    }
    return false;
}

bool vmm_query(const struct vmm *space, uint64_t address, uint64_t *physical, uint64_t *flags) {
    uint64_t parents;
    volatile uint64_t *slot = leaf(space, address, &parents);
    if (slot == NULL || (*slot & PRESENT) == 0) { return false; }
    uint64_t entry = *slot;
    if (physical != NULL) { *physical = (entry & ADDRESS_MASK) | (address & 4095); }
    if (flags != NULL) {
        *flags = (entry & ALLOWED & ~(VM_WRITE | VM_USER | VM_NX)) |
                 (entry & parents & (VM_WRITE | VM_USER)) | ((entry | parents) & VM_NX);
    }
    return true;
}

bool vmm_protect(struct vmm *space, uint64_t address, uint64_t flags) {
    if (space == NULL || !valid_page(address) || !valid_flags(flags) ||
        (space->shared_upper && address >= UINT64_C(0xffff800000000000))) { return false; }
    volatile uint64_t *slot = leaf(space, address, NULL);
    if (slot == NULL || (*slot & PRESENT) == 0) { return false; }
    if ((*slot & VM_USER) != (flags & VM_USER)) { return false; } /* No privilege changes in protect. */
    /* Changing an existing cache policy needs cache/alias coordination beyond
     * INVLPG. This API changes access permissions only; keep cache type fixed. */
    const uint64_t cache = VM_WRITE_THROUGH | VM_CACHE_DISABLE;
    if ((*slot & cache) != (flags & cache)) { return false; }
    *slot = (*slot & (ADDRESS_MASK | ACCESSED_DIRTY)) | flags | PRESENT;
    if (space->active) { space->ops.invalidate(space->ops.context, address); }
    return true;
}

bool vmm_unmap(struct vmm *space, uint64_t address, uint64_t *physical) {
    if (space == NULL || !space->ready || !valid_page(address) ||
        (space->shared_upper && address >= UINT64_C(0xffff800000000000))) { return false; }
    uint64_t frames[4];
    volatile uint64_t *parents[3];
    frames[0] = space->root;
    volatile uint64_t *entries = table(space, space->root);
    for (size_t level = 0; level < 3; ++level) {
        if (entries == NULL) { return false; }
        unsigned int shift = 39u - (unsigned int)level * 9u;
        parents[level] = &entries[(address >> shift) & 511];
        uint64_t entry = *parents[level];
        if ((entry & PRESENT) == 0 || (entry & LARGE) != 0) { return false; }
        frames[level + 1] = entry & ADDRESS_MASK;
        entries = table(space, frames[level + 1]);
    }
    if (entries == NULL) { return false; }
    volatile uint64_t *slot = &entries[(address >> 12) & 511];
    if ((*slot & PRESENT) == 0) { return false; }
    uint64_t data = *slot & ADDRESS_MASK;
    *slot = 0;
    --space->mapped_pages;
    if (space->active) { space->ops.invalidate(space->ops.context, address); }
    if (physical != NULL) { *physical = data; }
    for (size_t level = 3; level != 0; --level) {
        entries = table(space, frames[level]);
        bool empty = true;
        for (size_t i = 0; i < 512; ++i) {
            if (entries[i] != 0) { empty = false; break; }
        }
        if (!empty) { break; }
        *parents[level - 1] = 0;
        release_table(space, frames[level]);
    }
    return true;
}

/* Destroy owned nodes only. A child root never walks/frees its borrowed upper half. */
static bool valid_tree(const struct vmm *space, uint64_t physical, unsigned int level) {
    volatile uint64_t *entries = table(space, physical);
    if (entries == NULL) { return false; }
    if (level > 1) {
        for (size_t i = 0; i < (level == 4 && space->shared_upper ? 256u : 512u); ++i) {
            uint64_t entry = entries[i];
            if ((entry & PRESENT) != 0 && ((entry & LARGE) != 0 ||
                !valid_tree(space, entry & ADDRESS_MASK, level - 1))) { return false; }
        }
    }
    return true;
}
static void release_tree(struct vmm *space, uint64_t physical, unsigned int level) {
    volatile uint64_t *entries = table(space, physical);
    if (level > 1) {
        for (size_t i = 0; i < (level == 4 && space->shared_upper ? 256u : 512u); ++i) {
            uint64_t entry = entries[i];
            if ((entry & PRESENT) != 0) { release_tree(space, entry & ADDRESS_MASK, level - 1); }
        }
    }
    release_table(space, physical);
}
bool vmm_destroy(struct vmm *space) {
    if (space == NULL || !space->ready || space->active || !valid_tree(space, space->root, 4)) {
        return false;
    }
    release_tree(space, space->root, 4);
    space->ready = false;
    space->mapped_pages = 0;
    space->root = 0;
    return true;
}
