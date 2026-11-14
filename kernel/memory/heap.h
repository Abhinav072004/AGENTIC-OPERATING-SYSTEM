#ifndef AGENTICOS_HEAP_H
#define AGENTICOS_HEAP_H

#include <stdbool.h>
#include <stddef.h>

/* Borrowed arena, bootstrap CPU only. Initialize once with no live allocations.
 * Payloads are uninitialized, 16-byte aligned. Zero-size allocation fails. */
struct heap { unsigned char *arena; size_t bytes; bool ready, corrupt; };
struct heap_stats { size_t allocated_blocks, free_blocks, free_bytes, largest_free; };

bool heap_init(struct heap *heap, void *arena, size_t bytes);
void *heap_alloc(struct heap *heap, size_t bytes);
/* NULL is a successful no-op. Interior, foreign and already freed pointers fail.
 * No caller-provided payload pointer is dereferenced by free. */
bool heap_free(struct heap *heap, void *pointer);
bool heap_get_stats(struct heap *heap, struct heap_stats *stats);

#endif
