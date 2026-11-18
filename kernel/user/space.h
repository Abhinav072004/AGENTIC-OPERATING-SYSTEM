#ifndef AGENTICOS_USER_SPACE_H
#define AGENTICOS_USER_SPACE_H
#include "elf.h"
#include "memory/vmm.h"
struct user_page { uint64_t address, physical; };
struct user_space {
    struct vmm vm;
    struct user_page pages[USER_PAGE_CAPACITY];
    unsigned int count;
    uint64_t info_physical, writable_address;
};
bool user_space_load(struct user_space *space, const struct vmm *kernel,
                     const void *image, size_t length, uint64_t pid, bool private_page);
bool user_space_destroy(struct user_space *space);
#endif
