#include "space.h"
#include "syscall/abi.h"
#include "runtime/memory.h"
static bool add_page(struct user_space *space, uint64_t address, uint64_t flags, uint64_t *physical) {
    if (space->count == USER_PAGE_CAPACITY) { return false; }
    uint64_t frame;
    if (!space->vm.ops.alloc_table(space->vm.ops.context, &frame)) { return false; }
    void *pointer = space->vm.ops.pointer(space->vm.ops.context, frame);
    if (pointer == NULL || (uintptr_t)pointer % 4096 != 0 || !vmm_map(&space->vm, address, frame, flags)) {
        space->vm.ops.free_table(space->vm.ops.context, frame);
        return false;
    }
    memset(pointer, 0, 4096);
    space->pages[space->count++] = (struct user_page){address, frame};
    if (physical != NULL) { *physical = frame; }
    return true;
}
bool user_space_destroy(struct user_space *space) {
    if (space == NULL || !space->vm.ready || space->vm.active) { return false; }
    while (space->count != 0) {
        const struct user_page *page = &space->pages[space->count - 1];
        uint64_t physical;
        if (!vmm_unmap(&space->vm, page->address, &physical) || physical != page->physical) { return false; }
        space->vm.ops.free_table(space->vm.ops.context, physical);
        --space->count;
    }
    if (!vmm_destroy(&space->vm)) { return false; }
    memset(space, 0, sizeof(*space));
    return true;
}
bool user_space_load(struct user_space *space, const struct vmm *kernel,
                     const void *image, size_t length, uint64_t pid, bool private_page) {
    struct elf_plan plan;
    if (space == NULL || space->vm.ready || !user_elf_plan(image, length, &plan) || !vmm_create_user(&space->vm, kernel)) { return false; }
    space->count = 0;
    space->writable_address = 0;
    for (unsigned int i = 0; i < plan.count; ++i) {
        const struct elf_segment *segment = &plan.segments[i];
        if ((segment->flags & VM_WRITE) != 0 && space->writable_address == 0) { space->writable_address = segment->address; }
        for (uint64_t offset = 0; offset < segment->memory_bytes; offset += 4096) {
            uint64_t physical;
            if (!add_page(space, segment->address + offset, segment->flags, &physical)) { goto failure; }
            if (offset < segment->file_bytes) {
                uint64_t amount = segment->file_bytes - offset;
                if (amount > 4096) { amount = 4096; }
                memcpy(space->vm.ops.pointer(space->vm.ops.context, physical),
                       (const unsigned char *)image + segment->offset + offset, (size_t)amount);
            }
        }
    }
    for (uint64_t address = USER_STACK_BOTTOM; address < USER_STACK_TOP; address += 4096) {
        if (!add_page(space, address, VM_USER | VM_WRITE | VM_NX, NULL)) { goto failure; }
    }
    if (!add_page(space, USER_INFO_ADDRESS, VM_USER | VM_NX, &space->info_physical)) { goto failure; }
    struct user_info *info = space->vm.ops.pointer(space->vm.ops.context, space->info_physical);
    *info = (struct user_info){AGENTICOS_USER_ABI, pid, 0};
    if (private_page) {
        uint64_t physical;
        if (!add_page(space, USER_PRIVATE_ADDRESS, VM_USER | VM_WRITE | VM_NX, &physical)) { goto failure; }
        *(uint64_t *)space->vm.ops.pointer(space->vm.ops.context, physical) = UINT64_C(0x13579bdf2468ace0);
    }
    return true;
failure:
    /* Owned, inactive structures must always be reclaimable after ordinary OOM. */
    (void)user_space_destroy(space);
    return false;
}
