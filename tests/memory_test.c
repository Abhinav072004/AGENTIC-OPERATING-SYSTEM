#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "memory/pmm.h"
#include "memory/heap.h"
#include "memory/vmm.h"

static void pmm_tests(void) {
    const struct memory_region regions[] = {
        {0, PAGE_SIZE * 4, true}, {PAGE_SIZE * 4, PAGE_SIZE * 2, false},
        {PAGE_SIZE * 6, PAGE_SIZE * 6, true},
    };
    uint8_t storage[64];
    memset(storage, 0xa5, sizeof(storage));
    size_t required;
    assert(pmm_storage_size(regions, 3, &required) && required == 4);
    struct pmm pmm = {0};
    assert(pmm_init(&pmm, regions, 3, storage + 8, required));
    assert(pmm.managed_frames == 10 && pmm.free_frames == 10);
    assert(pmm_reserve(&pmm, PAGE_SIZE, PAGE_SIZE * 2));
    assert(pmm.managed_frames == 8 && pmm.free_frames == 8);
    assert(!pmm_reserve(&pmm, PAGE_SIZE, PAGE_SIZE));
    uint64_t allocated[12], physical;
    size_t count = 0;
    while (pmm_alloc_frame(&pmm, &physical)) {
        assert(count < 8 && physical % PAGE_SIZE == 0);
        assert(physical != PAGE_SIZE && physical != PAGE_SIZE * 2 &&
               physical != PAGE_SIZE * 4 && physical != PAGE_SIZE * 5);
        for (size_t i = 0; i < count; ++i) { assert(allocated[i] != physical); }
        allocated[count++] = physical;
    }
    assert(count == 8 && allocated[0] == 0 && pmm.free_frames == 0);
    assert(!pmm_alloc_frame(&pmm, &physical));
    assert(!pmm_free_frame(&pmm, 1) && !pmm_free_frame(&pmm, PAGE_SIZE));
    assert(!pmm_free_frame(&pmm, UINT64_MAX) && !pmm_free_frame(&pmm, PAGE_SIZE * 12));
    assert(!pmm_reserve(&pmm, 0, PAGE_SIZE));
    for (size_t i = count; i != 0; --i) { assert(pmm_free_frame(&pmm, allocated[i - 1])); }
    assert(!pmm_free_frame(&pmm, allocated[0]) && pmm.free_frames == 8);
    /* Mixed free/reserved range must reject atomically. */
    assert(!pmm_reserve(&pmm, 0, PAGE_SIZE * 3) && pmm.free_frames == 8);
    count = 0;
    while (pmm_alloc_frame(&pmm, &physical)) { assert(count < 8); allocated[count++] = physical; }
    assert(count == 8);
    for (size_t i = 0; i < count; ++i) { assert(pmm_free_frame(&pmm, allocated[i])); }
    for (size_t i = 0; i < sizeof(storage); ++i) {
        if (i < 8 || i >= 8 + required) { assert(storage[i] == 0xa5); }
    }
    struct memory_region invalid[] = {{0, PAGE_SIZE * 8, true}, {PAGE_SIZE, PAGE_SIZE, false}};
    assert(!pmm_storage_size(invalid, 2, &required));
    invalid[0].base = UINT64_MAX - 1; invalid[0].length = 4;
    assert(!pmm_storage_size(invalid, 1, &required));
    assert(!pmm_storage_size(NULL, 1, &required));
    assert(!pmm_storage_size(regions, PMM_REGION_LIMIT + 1, &required));
    assert(pmm_storage_size(regions, 3, &required));
    assert(!pmm_init(&pmm, regions, 3, storage, required - 1));
    assert(!pmm_init(&pmm, regions, 3, (void *)(UINTPTR_MAX - 1), required));
    const struct memory_region partial = {1, PAGE_SIZE * 3, true};
    assert(pmm_init(&pmm, &partial, 1, storage, sizeof(storage)) && pmm.free_frames == 2);
    assert(pmm_alloc_frame(&pmm, &physical) && physical == PAGE_SIZE);
    assert(pmm_alloc_frame(&pmm, &physical) && physical == PAGE_SIZE * 2);
    assert(!pmm_alloc_frame(&pmm, &physical));
    puts("PASS: PMM alignment, reservations, holes, unique exhaustion/recovery and invalid frees");
}

static void heap_tests(void) {
    const size_t capacity = 8192;
    void *arena = aligned_alloc(16, capacity);
    assert(arena != NULL);
    struct heap heap = {0};
    assert(heap_init(&heap, arena, capacity));
    struct heap_stats initial, stats;
    assert(heap_get_stats(&heap, &initial));
    void *a = heap_alloc(&heap, 1), *b = heap_alloc(&heap, 65);
    assert(a != NULL && b != NULL && (uintptr_t)a % 16 == 0 && (uintptr_t)b % 16 == 0);
    memset(b, 0x5a, 65);
    assert(!heap_free(&heap, (unsigned char *)b + 1));
    int foreign = 0;
    assert(!heap_free(&heap, &foreign));
    assert(heap_free(&heap, a) && !heap_free(&heap, a));
    for (size_t i = 0; i < 65; ++i) { assert(((unsigned char *)b)[i] == 0x5a); }
    assert(heap_free(&heap, b) && heap_free(&heap, NULL));
    assert(heap_alloc(&heap, 0) == NULL && heap_alloc(&heap, SIZE_MAX) == NULL);
    assert(heap_alloc(&heap, capacity) == NULL);
    assert(heap_get_stats(&heap, &stats) && stats.free_bytes == initial.free_bytes);
    void *fragments[4];
    for (size_t i = 0; i < 4; ++i) { fragments[i] = heap_alloc(&heap, 2000); assert(fragments[i]); }
    assert(heap_free(&heap, fragments[0]) && heap_free(&heap, fragments[2]));
    assert(heap_get_stats(&heap, &stats) && stats.free_bytes >= 3008 && stats.largest_free < 3008);
    assert(heap_alloc(&heap, 3000) == NULL); /* Enough total space, no large enough hole. */
    assert(heap_free(&heap, fragments[1]));
    void *merged = heap_alloc(&heap, 3000);
    assert(merged != NULL && heap_free(&heap, merged) && heap_free(&heap, fragments[3]));
    assert(heap_get_stats(&heap, &stats) && stats.largest_free == initial.largest_free);
    void *slots[32] = {0};
    size_t sizes[32] = {0};
    uint32_t random = UINT32_C(0x31415926);
    for (unsigned int step = 0; step < 2048; ++step) {
        for (size_t i = 0; i < 32; ++i) {
            if (slots[i] == NULL) { continue; }
            for (size_t j = 0; j < sizes[i]; ++j) {
                assert(((unsigned char *)slots[i])[j] == (unsigned char)(i + 1));
            }
        }
        random = random * UINT32_C(1664525) + UINT32_C(1013904223);
        size_t slot = (random >> 16) % 32;
        if (slots[slot] != NULL) { assert(heap_free(&heap, slots[slot])); slots[slot] = NULL; }
        else {
            size_t size = (random & 255) + 1;
            slots[slot] = heap_alloc(&heap, size);
            if (slots[slot] != NULL) {
                assert((uintptr_t)slots[slot] % 16 == 0);
                sizes[slot] = size;
                memset(slots[slot], (int)(slot + 1), size);
            } else {
                assert(heap_get_stats(&heap, &stats));
                assert(stats.largest_free < ((size + 15) & ~(size_t)15));
            }
        }
    }
    for (size_t i = 0; i < 32; ++i) { if (slots[i] != NULL) { assert(heap_free(&heap, slots[i])); } }
    assert(heap_get_stats(&heap, &stats) && stats.free_blocks == 1 && stats.allocated_blocks == 0);
    assert(stats.largest_free == initial.largest_free);
    ((unsigned char *)arena)[0] ^= 1;
    assert(heap_alloc(&heap, 16) == NULL && heap.corrupt && !heap_get_stats(&heap, &stats));
    free(arena);
    puts("PASS: heap alignment, fragmentation/coalescing, invalid frees, 2048 stress steps and corruption");
}

#define RAM_BASE UINT64_C(0x100000)
#define RAM_FRAMES ((size_t)64)
struct fixture {
    struct pmm pmm;
    uint8_t bitmap[128];
    unsigned char *ram;
    int budget;
    unsigned int invalidations;
};
static bool allocate_table(void *context, uint64_t *physical) {
    struct fixture *f = context;
    if (f->budget == 0) { return false; }
    if (f->budget > 0) { --f->budget; }
    return pmm_alloc_frame(&f->pmm, physical);
}
static void free_table(void *context, uint64_t physical) {
    struct fixture *f = context;
    assert(pmm_free_frame(&f->pmm, physical));
}
static void *physical_pointer(void *context, uint64_t physical) {
    struct fixture *f = context;
    if (physical < RAM_BASE || physical - RAM_BASE >= RAM_FRAMES * PAGE_SIZE ||
        !pmm_is_allocated(&f->pmm, physical)) { return NULL; }
    return f->ram + (physical - RAM_BASE);
}
static void invalidate(void *context, uint64_t address) {
    struct fixture *f = context;
    assert(address % PAGE_SIZE == 0);
    ++f->invalidations;
}
static void vmm_tests(void) {
    struct fixture f = {0};
    f.ram = aligned_alloc((size_t)PAGE_SIZE, RAM_FRAMES * (size_t)PAGE_SIZE);
    assert(f.ram != NULL);
    const struct memory_region region = {RAM_BASE, RAM_FRAMES * PAGE_SIZE, true};
    assert(pmm_init(&f.pmm, &region, 1, f.bitmap, sizeof(f.bitmap)));
    assert(pmm_reserve(&f.pmm, RAM_BASE, PAGE_SIZE));
    uint64_t available = f.pmm.free_frames;
    const struct vmm_ops ops = {&f, allocate_table, free_table, physical_pointer, invalidate};
    struct vmm space = {0};
    assert(!vmm_create(&space, &ops) && !space.ready && f.pmm.free_frames == available);
    f.budget = -1;
    assert(vmm_create(&space, &ops));
    uint64_t data;
    assert(pmm_alloc_frame(&f.pmm, &data));
    uint64_t before = f.pmm.free_frames;
    const uint64_t address = UINT64_C(0xffffd10000000000);
    for (int budget = 0; budget < 3; ++budget) {
        f.budget = budget;
        assert(!vmm_map(&space, address, data, VM_WRITE | VM_NX));
        assert(space.table_pages == 1 && space.mapped_pages == 0 && f.pmm.free_frames == before);
        assert(!vmm_query(&space, address, NULL, NULL));
    }
    f.budget = -1;
    assert(!vmm_map(&space, 0, data, VM_NX));
    assert(!vmm_map(&space, address + 1, data, VM_NX));
    assert(!vmm_map(&space, UINT64_C(0x0000800000000000), data, VM_NX));
    assert(!vmm_map(&space, address, data + 1, VM_NX));
    assert(!vmm_map(&space, address, UINT64_C(1) << 52, VM_NX));
    assert(!vmm_map(&space, address, data, VM_WRITE));
    assert(!vmm_map(&space, address, data, UINT64_C(4)) && f.pmm.free_frames == before);
    assert(vmm_map(&space, address, data, VM_WRITE | VM_NX));
    assert(space.table_pages == 4 && space.mapped_pages == 1);
    assert(!vmm_map(&space, address, data, VM_NX));
    uint64_t translated, flags;
    assert(vmm_query(&space, address + 123, &translated, &flags));
    assert(translated == data + 123 && flags == (VM_WRITE | VM_NX));
    assert(vmm_map(&space, address + PAGE_SIZE, data, VM_WRITE | VM_NX));
    assert(space.table_pages == 4 && space.mapped_pages == 2);
    space.active = true;
    assert(vmm_protect(&space, address, VM_NX) && f.invalidations == 1);
    assert(vmm_query(&space, address, &translated, &flags) && flags == VM_NX);
    assert(!vmm_protect(&space, address, VM_NX | VM_CACHE_DISABLE));
    assert(!vmm_protect(&space, address, VM_WRITE));
    assert(f.invalidations == 1);
    assert(vmm_query(&space, address, &translated, &flags) && flags == VM_NX);
    assert(!vmm_destroy(&space));
    assert(vmm_unmap(&space, address, &translated) && translated == data);
    assert(f.invalidations == 2 && space.table_pages == 4);
    assert(vmm_query(&space, address + PAGE_SIZE, &translated, &flags) && translated == data);
    assert(vmm_unmap(&space, address + PAGE_SIZE, &translated) && translated == data);
    assert(f.invalidations == 3 && space.table_pages == 1 && f.pmm.free_frames == before);
    assert(pmm_is_allocated(&f.pmm, data));
    assert(!vmm_unmap(&space, address, NULL) && !vmm_protect(&space, address, VM_NX));
    space.active = false;
    assert(vmm_destroy(&space) && space.table_pages == 0 && !vmm_destroy(&space));
    assert(vmm_create(&space, &ops) && vmm_map(&space, address, data, VM_NX));
    assert(vmm_destroy(&space) && pmm_is_allocated(&f.pmm, data));
    assert(pmm_free_frame(&f.pmm, data) && f.pmm.free_frames == available);
    free(f.ram);
    puts("PASS: VMM flags/translation, TLB hooks, OOM rollback, frame ownership and table cleanup");
}
int main(void) { pmm_tests(); heap_tests(); vmm_tests(); return 0; }
