#ifndef AGENTICOS_USER_COPY_H
#define AGENTICOS_USER_COPY_H
#include <stddef.h>
#include "memory/vmm.h"
bool user_buffer_valid(const struct vmm *space, uint64_t address, size_t length, bool writable);
bool copy_from_user(const struct vmm *space, void *destination, uint64_t source, size_t length);
bool copy_to_user(const struct vmm *space, uint64_t destination, const void *source, size_t length);
#endif
