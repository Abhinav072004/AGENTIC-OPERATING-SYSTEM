#include "heap.h"
#include <stdint.h>

#define ALIGNMENT ((size_t)16)
#define BLOCK_MAGIC UINT64_C(0x4147454e54494348)
struct block { uint64_t magic; size_t size, next; uint64_t used; };
_Static_assert(sizeof(struct block) % ALIGNMENT == 0, "aligned heap headers");

static struct block *at(struct heap *heap, size_t offset) {
    return (struct block *)(void *)(heap->arena + offset);
}

static bool validate(struct heap *heap) {
    if (heap == NULL || !heap->ready || heap->corrupt) { return false; }
    size_t offset = 0;
    for (;;) {
        if (offset % ALIGNMENT != 0 || offset > heap->bytes - sizeof(struct block)) { break; }
        const struct block *block = at(heap, offset);
        if (block->magic != BLOCK_MAGIC || block->used > 1 || block->size < ALIGNMENT ||
            block->size % ALIGNMENT != 0 ||
            block->size > heap->bytes - offset - sizeof(struct block)) { break; }
        size_t end = offset + sizeof(struct block) + block->size;
        if (end == heap->bytes) {
            if (block->next == 0) { return true; }
            break;
        }
        if (block->next != end) { break; }
        offset = end; /* Strict progress excludes cycles and gaps. */
    }
    heap->corrupt = true;
    return false;
}

bool heap_init(struct heap *heap, void *arena, size_t bytes) {
    if (heap == NULL) { return false; }
    heap->ready = false;
    heap->corrupt = false;
    bytes -= bytes % ALIGNMENT;
    if (arena == NULL || (uintptr_t)arena % ALIGNMENT != 0 ||
        bytes < sizeof(struct block) + ALIGNMENT ||
        (uintptr_t)arena > UINTPTR_MAX - bytes) { return false; }
    heap->arena = arena;
    heap->bytes = bytes;
    struct block *first = at(heap, 0);
    first->magic = BLOCK_MAGIC;
    first->size = bytes - sizeof(struct block);
    first->next = 0;
    first->used = 0;
    heap->ready = true;
    return true;
}

void *heap_alloc(struct heap *heap, size_t bytes) {
    if (bytes == 0 || bytes > SIZE_MAX - (ALIGNMENT - 1) || !validate(heap)) { return NULL; }
    size_t wanted = (bytes + ALIGNMENT - 1) & ~(ALIGNMENT - 1);
    size_t offset = 0;
    for (;;) {
        struct block *block = at(heap, offset);
        if (block->used == 0 && block->size >= wanted) {
            if (block->size - wanted >= sizeof(struct block) + ALIGNMENT) {
                size_t split_offset = offset + sizeof(struct block) + wanted;
                struct block *split = at(heap, split_offset);
                split->magic = BLOCK_MAGIC;
                split->size = block->size - wanted - sizeof(struct block);
                split->next = block->next;
                split->used = 0;
                block->next = split_offset;
                block->size = wanted;
            }
            block->used = 1;
            return heap->arena + offset + sizeof(struct block);
        }
        if (block->next == 0) { return NULL; }
        offset = block->next;
    }
}

bool heap_free(struct heap *heap, void *pointer) {
    if (pointer == NULL) { return true; }
    if (!validate(heap)) { return false; }
    size_t offset = 0;
    bool found = false;
    for (;;) {
        struct block *block = at(heap, offset);
        if ((uintptr_t)pointer == (uintptr_t)heap->arena + offset + sizeof(struct block)) {
            if (block->used == 0) { return false; }
            block->used = 0;
            found = true;
            break;
        }
        if (block->next == 0) { break; }
        offset = block->next;
    }
    if (!found) { return false; }
    offset = 0;
    for (;;) {
        struct block *block = at(heap, offset);
        if (block->next == 0) { break; }
        struct block *next = at(heap, block->next);
        if (block->used == 0 && next->used == 0) {
            block->size += sizeof(struct block) + next->size;
            block->next = next->next;
            next->magic = 0;
        } else { offset = block->next; }
    }
    return true;
}

bool heap_get_stats(struct heap *heap, struct heap_stats *stats) {
    if (stats == NULL || !validate(heap)) { return false; }
    stats->allocated_blocks = 0;
    stats->free_blocks = 0;
    stats->free_bytes = 0;
    stats->largest_free = 0;
    size_t offset = 0;
    for (;;) {
        const struct block *block = at(heap, offset);
        if (block->used != 0) { ++stats->allocated_blocks; }
        else {
            ++stats->free_blocks;
            stats->free_bytes += block->size;
            if (block->size > stats->largest_free) { stats->largest_free = block->size; }
        }
        if (block->next == 0) { return true; }
        offset = block->next;
    }
}
