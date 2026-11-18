#ifndef AGENTICOS_SYSCALL_ABI_H
#define AGENTICOS_SYSCALL_ABI_H
#include <stdint.h>
#define AGENTICOS_USER_ABI UINT64_C(1)
#define SYS_WRITE UINT64_C(1)
#define SYS_READ UINT64_C(2)
#define SYS_EXIT UINT64_C(3)
#define SYS_GETPID UINT64_C(4)
#define SYS_YIELD UINT64_C(5)
#define SYS_OPEN UINT64_C(6)
#define SYS_CLOSE UINT64_C(7)
#define SYS_SEEK UINT64_C(8)
#define SYS_MKDIR UINT64_C(9)
#define SYS_UNLINK UINT64_C(10)
#define SYS_RENAME UINT64_C(11)
#define SYS_STAT UINT64_C(12)
#define SYS_LIST UINT64_C(13)
#define SYS_COUNT 64u
#define SYS_IO_MAX 128u
#define SYS_E2BIG (-7L)
#define SYS_EBADF (-9L)
#define SYS_EFAULT (-14L)
#define SYS_EINVAL (-22L)
#define SYS_ENOSYS (-38L)
struct user_info { uint64_t abi, pid, ticks; };
#endif
