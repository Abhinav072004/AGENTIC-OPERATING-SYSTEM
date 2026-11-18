#include "copy.h"
#include "elf.h"
#include "runtime/memory.h"
bool user_buffer_valid(const struct vmm *space, uint64_t address, size_t length, bool writable) {
    if (space == NULL || !space->ready || !space->shared_upper) { return false; }
    if (length == 0) { return true; }
    if (address < 4096 || address >= USER_LIMIT || length > USER_LIMIT - address) { return false; }
    uint64_t end = address + length;
    for (uint64_t page = address & ~UINT64_C(4095); page < end; page += 4096) {
        uint64_t physical, flags;
        if (!vmm_query(space, page, &physical, &flags) || (flags & VM_USER) == 0 ||
            (writable && (flags & VM_WRITE) == 0) || space->ops.pointer(space->ops.context, physical) == NULL) { return false; }
    }
    return true;
}
static bool copy_buffer(const struct vmm *space, uint64_t address, void *buffer, size_t length, bool to_user) {
    if ((buffer == NULL && length != 0) || !user_buffer_valid(space, address, length, to_user)) { return false; }
    size_t copied = 0;
    while (copied < length) {
        uint64_t physical;
        if (!vmm_query(space, address + copied, &physical, NULL)) { return false; }
        size_t offset = (size_t)(physical & 4095), chunk = 4096 - offset;
        if (chunk > length - copied) { chunk = length - copied; }
        unsigned char *page = space->ops.pointer(space->ops.context, physical & ~UINT64_C(4095));
        if (to_user) { memcpy(page + offset, (const unsigned char *)buffer + copied, chunk); }
        else { memcpy((unsigned char *)buffer + copied, page + offset, chunk); }
        copied += chunk;
    }
    return true;
}
bool copy_from_user(const struct vmm *space, void *destination, uint64_t source, size_t length) {
    return copy_buffer(space, source, destination, length, false);
}
bool copy_to_user(const struct vmm *space, uint64_t destination, const void *source, size_t length) {
    return copy_buffer(space, destination, (void *)source, length, true);
}
