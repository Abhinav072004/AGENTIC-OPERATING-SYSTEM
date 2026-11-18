#ifndef AGENTICOS_USER_RUNTIME_H
#define AGENTICOS_USER_RUNTIME_H
#include <stddef.h>
#include "syscall/abi.h"
#include "user/elf.h"
static inline long user_call(uint64_t number, uint64_t first, uint64_t second, uint64_t third) {
    register uint64_t result __asm__("rax") = number;
    __asm__ volatile ("int $0x82" : "+a"(result) : "D"(first), "S"(second), "d"(third) : "memory", "cc");
    return (long)result;
}
static inline long user_write(uint64_t fd, const void *buffer, size_t length) {
    return user_call(SYS_WRITE, fd, (uintptr_t)buffer, length);
}
static inline long user_read(uint64_t fd, void *buffer, size_t length) {
    return user_call(SYS_READ, fd, (uintptr_t)buffer, length);
}
#endif
