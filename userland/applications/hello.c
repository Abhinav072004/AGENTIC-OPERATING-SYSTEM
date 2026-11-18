#include "syscall.h"
long user_main(uint64_t expected_pid, uint64_t mode);
long user_main(uint64_t expected_pid, uint64_t mode) {
    (void)mode;
    uint16_t cs;
    __asm__ volatile ("mov %%cs, %0" : "=r"(cs));
    if ((cs & 3u) != 3 || user_call(SYS_GETPID, 0, 0, 0) != (long)expected_pid) { return 91; }
    static const char message[] = "Hello from native AgenticOS user mode";
    if (user_write(1, message, sizeof(message) - 1) != (long)(sizeof(message) - 1)) { return 92; }
    if (user_call(SYS_YIELD, 0, 0, 0) != 0) { return 93; }
    return 0;
}
