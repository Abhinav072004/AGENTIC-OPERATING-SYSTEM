#ifndef AGENTICOS_USER_ELF_H
#define AGENTICOS_USER_ELF_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define USER_IMAGE_BASE UINT64_C(0x400000)
#define USER_IMAGE_END UINT64_C(0x800000)
#define USER_INFO_ADDRESS UINT64_C(0x50000000)
#define USER_PRIVATE_ADDRESS UINT64_C(0x60000000)
#define USER_STACK_BOTTOM UINT64_C(0x70000000)
#define USER_STACK_TOP (USER_STACK_BOTTOM + UINT64_C(0x4000))
#define USER_LIMIT UINT64_C(0x800000000000)
#define USER_PAGE_CAPACITY 64u
#define ELF_SEGMENT_CAPACITY 8u
struct elf_segment { uint64_t address, offset, file_bytes, memory_bytes, flags; };
struct elf_plan { uint64_t entry; unsigned int count, pages; struct elf_segment segments[ELF_SEGMENT_CAPACITY]; };
bool user_elf_plan(const void *image, size_t length, struct elf_plan *plan);
#endif
