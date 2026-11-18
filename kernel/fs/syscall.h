#ifndef AGENTICOS_FS_SYSCALL_H
#define AGENTICOS_FS_SYSCALL_H
#include "user/user.h"
long fs_syscall(struct user_process *process, uint64_t number, uint64_t a, uint64_t b, uint64_t c);
#endif
