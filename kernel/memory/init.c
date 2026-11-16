#include "init.h"
#include "arch/x86_64/memory.h"
#include "debug/log.h"
#include "debug/panic.h"
#include "arch/x86_64/io.h"

extern unsigned char __kernel_start[], __text_start[], __rodata_start[];
extern unsigned char __data_start[], __kernel_end[];
static struct pmm frames;
static struct vmm address_space;
static struct heap kernel_heap;
static uint64_t direct_offset, metadata_physical;

static bool memory_error(const char *reason) { klog(LOG_ERROR, "memory", "%s", reason); return false; }
struct pmm *memory_frames(void) { return &frames; }
struct vmm *memory_address_space(void) { return &address_space; }
void *memory_frame_pointer(uint64_t physical) {
    if (!pmm_is_allocated(&frames, physical) || physical >= MEMORY_DIRECT_LIMIT) { return NULL; }
    return (void *)(uintptr_t)(direct_offset + physical);
}
static bool alloc_table(void *context, uint64_t *physical) { return pmm_alloc_frame(context, physical); }
static void free_table(void *context, uint64_t physical) { KASSERT(pmm_free_frame(context, physical)); }
static void *table_pointer(void *context, uint64_t physical) {
    (void)context;
    return memory_frame_pointer(physical);
}
static void invalidate(void *context, uint64_t address) { (void)context; arch_memory_invalidate(address); }

static bool direct_region(uint64_t base, uint64_t length, uint64_t flags) {
    if (base > MEMORY_DIRECT_LIMIT || length > MEMORY_DIRECT_LIMIT - base) { return false; }
    uint64_t first = base - base % PAGE_SIZE, end = base + length;
    if (end % PAGE_SIZE != 0) { end += PAGE_SIZE - end % PAGE_SIZE; }
    for (uint64_t physical = first; physical < end; physical += PAGE_SIZE) {
        uint64_t address = direct_offset + physical, existing_physical, existing_flags;
        if (vmm_query(&address_space, address, &existing_physical, &existing_flags)) {
            if (existing_physical != physical || existing_flags != flags) { return false; }
        } else if (!vmm_map(&address_space, address, physical, flags)) { return false; }
    }
    return true;
}
static bool kernel_range(const struct memory_boot *boot, uint64_t start,
                         uint64_t end, uint64_t flags) {
    if (start < boot->kernel_virtual || end < start) { return false; }
    for (uint64_t address = start; address < end; address += PAGE_SIZE) {
        uint64_t physical = boot->kernel_physical + (address - boot->kernel_virtual);
        if (!vmm_map(&address_space, address, physical, flags)) { return false; }
        if ((flags & VM_WRITE) == 0 &&
            !vmm_protect(&address_space, direct_offset + physical, VM_NX)) { return false; }
    }
    return true;
}

bool memory_bootstrap(const struct memory_boot *boot) {
    if (frames.ready || address_space.ready || kernel_heap.ready) {
        return memory_error("memory bootstrap must only run once");
    }
    if (boot == NULL || boot->hhdm != MEMORY_HHDM_BASE ||
        boot->kernel_virtual != (uintptr_t)__kernel_start || boot->kernel_physical % PAGE_SIZE != 0 ||
        boot->kernel_physical >= MEMORY_DIRECT_LIMIT ||
        (uintptr_t)__kernel_end - boot->kernel_virtual > MEMORY_DIRECT_LIMIT - boot->kernel_physical) {
        return memory_error("unsupported bootstrap memory layout");
    }
    size_t bitmap_bytes;
    if (!pmm_storage_size(boot->regions, boot->count, &bitmap_bytes) ||
        bitmap_bytes > SIZE_MAX - ((size_t)PAGE_SIZE - 1)) {
        return memory_error("invalid memory map or bitmap size");
    }
    uint64_t reserved = ((uint64_t)bitmap_bytes + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    bool found = false;
    for (size_t i = 0; i < boot->count; ++i) {
        const struct memory_region *region = &boot->regions[i];
        if ((region->usable || boot->direct_flags[i] != MEMORY_SKIP_REGION) &&
            (region->base > MEMORY_DIRECT_LIMIT || region->length > MEMORY_DIRECT_LIMIT - region->base)) {
            return memory_error("memory exceeds direct-map limit");
        }
        if (!region->usable || found) { continue; }
        uint64_t first = (region->base / PAGE_SIZE +
                          (uint64_t)(region->base % PAGE_SIZE != 0)) * PAGE_SIZE;
        uint64_t end = (region->base + region->length) / PAGE_SIZE * PAGE_SIZE;
        if (first <= end && reserved <= end - first) { metadata_physical = first; found = true; }
    }
    if (!found) { return memory_error("no usable range can hold PMM metadata"); }
    direct_offset = boot->hhdm;
    void *storage = (void *)(uintptr_t)(direct_offset + metadata_physical);
    if (!pmm_init(&frames, boot->regions, boot->count, storage, bitmap_bytes) ||
        !pmm_reserve(&frames, metadata_physical, reserved)) { return memory_error("cannot reserve PMM metadata"); }
    klog(LOG_INFO, "memory", "PMM managed=%llu free=%llu frames; page_size=4096",
         (unsigned long long)frames.managed_frames, (unsigned long long)frames.free_frames);
    const struct vmm_ops ops = {&frames, alloc_table, free_table, table_pointer, invalidate};
    if (!vmm_create(&address_space, &ops)) { return memory_error("no frame available for kernel root"); }
    for (size_t i = 0; i < boot->count; ++i) {
        if (boot->direct_flags[i] != MEMORY_SKIP_REGION && boot->regions[i].length != 0 &&
            !direct_region(boot->regions[i].base, boot->regions[i].length, boot->direct_flags[i])) {
            KASSERT(vmm_destroy(&address_space));
            return memory_error("cannot map direct-memory region");
        }
    }
    if (!kernel_range(boot, (uintptr_t)__kernel_start, (uintptr_t)__text_start, VM_WRITE | VM_NX) ||
        !kernel_range(boot, (uintptr_t)__text_start, (uintptr_t)__rodata_start, 0) ||
        !kernel_range(boot, (uintptr_t)__rodata_start, (uintptr_t)__data_start, VM_NX) ||
        !kernel_range(boot, (uintptr_t)__data_start, (uintptr_t)__kernel_end, VM_WRITE | VM_NX)) {
        KASSERT(vmm_destroy(&address_space));
        return memory_error("cannot map/protect kernel segments");
    }
    klog(LOG_INFO, "memory", "activating kernel CR3=%p with %llu table frames",
         (void *)(uintptr_t)address_space.root, (unsigned long long)address_space.table_pages);
    bool activated = arch_memory_activate(address_space.root);
    address_space.active = arch_memory_root() == address_space.root;
    if (!activated) { return memory_error("CPU paging/NX/WP activation failed"); }
    KASSERT(arch_memory_protection_enabled());
    size_t mapped = 0;
    for (; mapped < MEMORY_HEAP_BYTES / (size_t)PAGE_SIZE; ++mapped) {
        uint64_t physical;
        if (!pmm_alloc_frame(&frames, &physical)) { break; }
        if (!vmm_map(&address_space, MEMORY_HEAP_START + mapped * PAGE_SIZE, physical, VM_WRITE | VM_NX)) {
            KASSERT(pmm_free_frame(&frames, physical));
            break;
        }
    }
    if (mapped != MEMORY_HEAP_BYTES / (size_t)PAGE_SIZE ||
        !heap_init(&kernel_heap, (void *)(uintptr_t)MEMORY_HEAP_START, MEMORY_HEAP_BYTES)) {
        while (mapped != 0) {
            uint64_t physical;
            --mapped;
            KASSERT(vmm_unmap(&address_space, MEMORY_HEAP_START + mapped * PAGE_SIZE, &physical));
            KASSERT(pmm_free_frame(&frames, physical));
        }
        return memory_error("cannot map/initialize kernel heap");
    }
    KASSERT(!vmm_query(&address_space, MEMORY_HEAP_GUARD, NULL, NULL));
    KASSERT(!vmm_query(&address_space, MEMORY_HEAP_START + MEMORY_HEAP_BYTES, NULL, NULL));
    klog(LOG_INFO, "memory", "heap=%zu bytes; guard pages=2; supervisor WP=1 NX=1", MEMORY_HEAP_BYTES);
    return true;
}

void *kmalloc(size_t bytes) {
    uint64_t flags = irq_save();
    void *result = heap_alloc(&kernel_heap, bytes);
    if (kernel_heap.corrupt) { PANIC("kernel heap metadata is corrupt"); }
    irq_restore(flags);
    return result;
}
bool kfree(void *pointer) {
    uint64_t flags = irq_save();
    bool result = heap_free(&kernel_heap, pointer);
    if (kernel_heap.corrupt) { PANIC("kernel heap metadata is corrupt"); }
    irq_restore(flags);
    return result;
}

void memory_self_test(void) {
    uint64_t initial = frames.free_frames, first, second, physical, flags;
    KASSERT(vmm_query(&address_space, (uintptr_t)__text_start, &physical, &flags) && flags == 0);
    KASSERT(vmm_query(&address_space, direct_offset + physical, NULL, &flags) && flags == VM_NX);
    KASSERT(vmm_query(&address_space, (uintptr_t)__data_start, NULL, &flags) && flags == (VM_WRITE | VM_NX));
    KASSERT(pmm_alloc_frame(&frames, &first));
    KASSERT(pmm_alloc_frame(&frames, &second) && first != second);
    KASSERT(!pmm_free_frame(&frames, first + 1));
    KASSERT(!pmm_free_frame(&frames, metadata_physical));
    volatile uint64_t *first_pointer = memory_frame_pointer(first);
    volatile uint64_t *second_pointer = memory_frame_pointer(second);
    KASSERT(first_pointer != NULL && second_pointer != NULL);
    *first_pointer = UINT64_C(0x123456789abcdef0);
    *second_pointer = UINT64_C(0xfedcba9876543210);
    KASSERT(vmm_map(&address_space, MEMORY_SCRATCH, first, VM_WRITE | VM_NX));
    volatile uint64_t *alias = (volatile uint64_t *)(uintptr_t)MEMORY_SCRATCH;
    KASSERT(*alias == *first_pointer);
    *alias = UINT64_C(0x0102030405060708);
    KASSERT(*first_pointer == UINT64_C(0x0102030405060708));
    KASSERT(vmm_protect(&address_space, MEMORY_SCRATCH, VM_NX));
    KASSERT(vmm_query(&address_space, MEMORY_SCRATCH, &physical, &flags) && physical == first && flags == VM_NX);
    KASSERT(*alias == UINT64_C(0x0102030405060708));
    KASSERT(vmm_unmap(&address_space, MEMORY_SCRATCH, &physical) && physical == first);
    KASSERT(!vmm_query(&address_space, MEMORY_SCRATCH, NULL, NULL));
    KASSERT(vmm_map(&address_space, MEMORY_SCRATCH, second, VM_WRITE | VM_NX));
    KASSERT(*alias == UINT64_C(0xfedcba9876543210)); /* Stale TLB mappings must not survive. */
    KASSERT(vmm_unmap(&address_space, MEMORY_SCRATCH, &physical) && physical == second);
    KASSERT(pmm_free_frame(&frames, first) && !pmm_free_frame(&frames, first));
    KASSERT(pmm_free_frame(&frames, second) && frames.free_frames == initial);
    klog(LOG_INFO, "memory-test", "PMM allocate/free and VMM alias/protect/remap/unmap passed");
    struct heap_stats before, after;
    KASSERT(heap_get_stats(&kernel_heap, &before));
    void *a = kmalloc(1), *b = kmalloc(4097), *c = kmalloc(65);
    KASSERT(a != NULL && b != NULL && c != NULL);
    KASSERT((uintptr_t)a % 16 == 0 && (uintptr_t)b % 16 == 0 && (uintptr_t)c % 16 == 0);
    volatile uint8_t *bytes = b;
    for (size_t i = 0; i < 4097; ++i) { bytes[i] = (uint8_t)i; }
    KASSERT(!kfree((uint8_t *)b + 1));
    KASSERT(kfree(a) && !kfree(a));
    for (size_t i = 0; i < 4097; ++i) { KASSERT(bytes[i] == (uint8_t)i); }
    KASSERT(kfree(c) && kfree(b));
    KASSERT(kmalloc(0) == NULL && kmalloc(SIZE_MAX) == NULL && kmalloc(MEMORY_HEAP_BYTES) == NULL);
    KASSERT(heap_get_stats(&kernel_heap, &after));
    KASSERT(after.allocated_blocks == 0 && after.free_blocks == 1 && after.free_bytes == before.free_bytes &&
             after.largest_free == before.largest_free);
    klog(LOG_INFO, "memory-test", "heap alignment, cross-page data, invalid-free and coalescing passed");
}

void memory_exhaustion_test(void) {
    uint64_t initial = frames.free_frames, head = UINT64_MAX, physical, count = 0;
    KASSERT(initial != 0);
    /* Store the recovery list inside allocated frames: OOM needs no heap list. */
    while (pmm_alloc_frame(&frames, &physical)) {
        uint64_t *node = memory_frame_pointer(physical);
        KASSERT(node != NULL && count < initial);
        *node = head;
        head = physical;
        ++count;
    }
    KASSERT(count == initial && frames.free_frames == 0 && !pmm_alloc_frame(&frames, &physical));
    uint64_t released = 0;
    while (head != UINT64_MAX) {
        uint64_t *node = memory_frame_pointer(head);
        KASSERT(node != NULL);
        uint64_t next = *node;
        KASSERT(pmm_free_frame(&frames, head));
        head = next;
        ++released;
    }
    KASSERT(released == initial && frames.free_frames == initial);
    klog(LOG_INFO, "memory-test", "exhausted and recovered %llu physical frames", (unsigned long long)released);
}
