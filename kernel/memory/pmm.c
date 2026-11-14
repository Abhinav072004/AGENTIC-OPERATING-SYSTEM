#include "pmm.h"

static bool bit(const uint8_t *bitmap, uint64_t index) {
    return (bitmap[index / 8] & (uint8_t)(1u << (index % 8))) != 0;
}

static void set_bit(uint8_t *bitmap, uint64_t index, bool value) {
    uint8_t mask = (uint8_t)(1u << (index % 8));
    if (value) { bitmap[index / 8] |= mask; }
    else { bitmap[index / 8] &= (uint8_t)~mask; }
}

static bool inspect(const struct memory_region *regions, size_t count,
                    uint64_t *frames, size_t *plane) {
    if (regions == NULL || count == 0 || count > PMM_REGION_LIMIT) { return false; }
    uint64_t highest = 0;
    for (size_t i = 0; i < count; ++i) {
        if (regions[i].base > UINT64_MAX - regions[i].length) { return false; }
        uint64_t end = regions[i].base + regions[i].length;
        for (size_t j = 0; j < i; ++j) {
            if (regions[i].length != 0 && regions[j].length != 0 &&
                regions[i].base < regions[j].base + regions[j].length &&
                regions[j].base < end) { return false; }
        }
        uint64_t first = regions[i].base / PAGE_SIZE +
                         (uint64_t)(regions[i].base % PAGE_SIZE != 0);
        uint64_t last = end / PAGE_SIZE;
        if (regions[i].usable && first < last && last > highest) { highest = last; }
    }
    if (highest == 0) { return false; }
    uint64_t bytes = highest / 8 + (uint64_t)(highest % 8 != 0);
    if (bytes > SIZE_MAX / 2) { return false; }
    *frames = highest;
    *plane = (size_t)bytes;
    return true;
}

bool pmm_storage_size(const struct memory_region *regions, size_t count, size_t *bytes) {
    uint64_t frames;
    size_t plane;
    if (bytes == NULL || !inspect(regions, count, &frames, &plane)) { return false; }
    *bytes = plane * 2;
    return true;
}

bool pmm_init(struct pmm *pmm, const struct memory_region *regions, size_t count,
              void *storage, size_t bytes) {
    if (pmm == NULL) { return false; }
    pmm->ready = false;
    uint64_t frames;
    size_t plane;
    if (storage == NULL || !inspect(regions, count, &frames, &plane) || bytes < plane * 2 ||
        (uintptr_t)storage > UINTPTR_MAX - plane * 2) { return false; }
    pmm->eligible = storage;
    pmm->used = (uint8_t *)storage + plane;
    for (size_t i = 0; i < plane * 2; ++i) { ((uint8_t *)storage)[i] = 0; }
    pmm->frame_count = frames;
    pmm->managed_frames = 0;
    pmm->cursor = 0;
    for (size_t i = 0; i < count; ++i) {
        if (!regions[i].usable) { continue; }
        uint64_t first = regions[i].base / PAGE_SIZE +
                         (uint64_t)(regions[i].base % PAGE_SIZE != 0);
        uint64_t last = (regions[i].base + regions[i].length) / PAGE_SIZE;
        for (uint64_t frame = first; frame < last; ++frame) {
            set_bit(pmm->eligible, frame, true);
            ++pmm->managed_frames;
        }
    }
    pmm->free_frames = pmm->managed_frames;
    pmm->ready = true;
    return true;
}

bool pmm_reserve(struct pmm *pmm, uint64_t physical, uint64_t bytes) {
    if (pmm == NULL || !pmm->ready || bytes == 0 ||
        physical % PAGE_SIZE != 0 || bytes % PAGE_SIZE != 0) { return false; }
    uint64_t first = physical / PAGE_SIZE, count = bytes / PAGE_SIZE;
    if (first >= pmm->frame_count || count > pmm->frame_count - first) { return false; }
    for (uint64_t frame = first; frame < first + count; ++frame) {
        if (!bit(pmm->eligible, frame) || bit(pmm->used, frame)) { return false; }
    }
    for (uint64_t frame = first; frame < first + count; ++frame) {
        set_bit(pmm->eligible, frame, false);
    }
    pmm->managed_frames -= count;
    pmm->free_frames -= count;
    return true;
}

bool pmm_alloc_frame(struct pmm *pmm, uint64_t *physical) {
    if (pmm == NULL || !pmm->ready || physical == NULL || pmm->free_frames == 0) { return false; }
    for (uint64_t step = 0; step < pmm->frame_count; ++step) {
        uint64_t frame = (pmm->cursor + step) % pmm->frame_count;
        if (bit(pmm->eligible, frame) && !bit(pmm->used, frame)) {
            set_bit(pmm->used, frame, true);
            --pmm->free_frames;
            pmm->cursor = (frame + 1) % pmm->frame_count;
            *physical = frame * PAGE_SIZE;
            return true;
        }
    }
    return false;
}

bool pmm_is_allocated(const struct pmm *pmm, uint64_t physical) {
    if (pmm == NULL || !pmm->ready || physical % PAGE_SIZE != 0) { return false; }
    uint64_t frame = physical / PAGE_SIZE;
    return frame < pmm->frame_count && bit(pmm->eligible, frame) && bit(pmm->used, frame);
}

bool pmm_free_frame(struct pmm *pmm, uint64_t physical) {
    if (!pmm_is_allocated(pmm, physical)) { return false; }
    set_bit(pmm->used, physical / PAGE_SIZE, false);
    ++pmm->free_frames;
    return true;
}
