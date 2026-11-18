#include "syscall.h"
long user_main(uint64_t expected_pid, uint64_t mode);
static volatile uint64_t cookie;
static unsigned char executable_data[16] = {0xc3};
static const char text[] = "native user checks passed";
long user_main(uint64_t expected_pid, uint64_t mode) {
    if (user_call(SYS_GETPID, 0, 0, 0) != (long)expected_pid) { return 100; }
    if (mode == 1) { return (long)*(volatile uint8_t *)(uintptr_t)UINT64_C(0xffffffff80000000); }
    if (mode == 2) { return (long)*(volatile uint64_t *)(uintptr_t)USER_PRIVATE_ADDRESS; }
    if (mode == 3) { __asm__ volatile ("movb $0, (%0)" : : "r"(text) : "memory"); return 103; }
    if (mode == 4) { __asm__ volatile ("call *%0" : : "r"(executable_data) : "memory"); return 104; }
    if (mode == 5) { __asm__ volatile ("cli"); return 105; }
    if (mode == 6) { __asm__ volatile ("int $0x81" : : : "memory"); return 106; }
    if (mode == 7) { __asm__ volatile ("pxor %xmm0, %xmm0"); return 107; }
    if (mode == 8) {
        __asm__ volatile ("movabs $0x800000000000, %rsp; mov $4, %eax; int $0x82; ud2");
        return 108;
    }
    if (mode == 9) {
        char character = 0;
        long result;
        do { result = user_read(0, &character, 1); if (result == 0) { (void)user_call(SYS_YIELD, 0, 0, 0); } }
        while (result == 0);
        if (result != 1 || character != 'Z' || user_write(1, &character, 1) != 1) { return 109; }
        return 0;
    }
    uint16_t cs, ss;
    uint64_t flags;
    __asm__ volatile ("mov %%cs, %0; mov %%ss, %1; pushfq; pop %2" : "=r"(cs), "=r"(ss), "=r"(flags));
    if (cs != 43 || ss != 51 || (flags & UINT64_C(0x3000)) != 0 || (flags & 0x200u) == 0) { return 110; }
    volatile const struct user_info *info = (const void *)(uintptr_t)USER_INFO_ADDRESS;
    if (info->abi != AGENTICOS_USER_ABI || info->pid != expected_pid || cookie != 0) { return 111; }
    cookie = expected_pid;
    if (user_call(999, 0, 0, 0) != SYS_ENOSYS || user_write(55, text, 1) != SYS_EBADF ||
        user_read(1, (void *)text, 1) != SYS_EBADF) { return 112; }
    if (user_write(1, NULL, 1) != SYS_EFAULT || user_write(1, (const void *)(uintptr_t)UINT64_MAX, 8) != SYS_EFAULT ||
        user_write(1, (const void *)(uintptr_t)UINT64_C(0xffffffff80000000), 1) != SYS_EFAULT) { return 113; }
    if (user_read(0, (void *)text, 1) != SYS_EFAULT ||
        user_write(1, (const void *)(uintptr_t)(USER_STACK_TOP - 2), 4) != SYS_EFAULT) { return 114; }
    if (user_write(1, text, SYS_IO_MAX + 1) != SYS_E2BIG ||
        user_write(1, (const void *)(uintptr_t)UINT64_MAX, 0) != 0 ||
        user_call(SYS_EXIT, UINT64_MAX >> 1, 0, 0) != SYS_EINVAL) { return 115; }
    char input[8] = {'x', 'x', 'x', 'x', 'x', 'x', 'x', 'x'};
    if (user_read(0, input, sizeof(input)) != 0 || input[0] != 'x' || input[7] != 'x') { return 116; }
    if (user_call(SYS_YIELD, 0, 0, 0) != 0) { return 117; }
    while (info->ticks < 6) {
        if (cookie != expected_pid) { return 118; }
        __asm__ volatile ("pause" : : : "memory");
    }
    volatile char full[SYS_IO_MAX];
    for (size_t i = 0; i < SYS_IO_MAX; ++i) { full[i] = 'u'; }
    if (cookie != expected_pid || user_write(1, text, sizeof(text) - 1) != (long)(sizeof(text) - 1) ||
        user_write(2, (const void *)full, sizeof(full)) != (long)sizeof(full)) { return 119; }
    return 0;
}
