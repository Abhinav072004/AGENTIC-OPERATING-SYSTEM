#include "abi.h"
#include "user/user.h"
#include "user/copy.h"
#include "scheduler/task.h"
#include "arch/x86_64/memory.h"
#include "arch/x86_64/serial.h"
#include "debug/log.h"
#include "debug/panic.h"
#include <limits.h>
#include "fs/syscall.h"
#include "system.h"

struct interrupt_frame *syscall_dispatch(struct interrupt_frame *frame) {
    KASSERT(task_is_user() && (frame->cs & 3u) == 3);
    struct user_process *process = user_find(task_pid());
    KASSERT(process != NULL && process->space.vm.active && arch_memory_root() == process->space.vm.root);
    uint64_t top = arch_interrupts_rsp0();
    KASSERT((uintptr_t)frame >= top - UINT64_C(16384) && (uintptr_t)frame + sizeof(*frame) <= top);
    if (!process->entered) {
        process->entered = true;
        klog(LOG_INFO, "user-enter", "case=%s pid=%llu cr3=%p cs=0x%llx ss=0x%llx rsp0=%p frame=%p",
             process->name, (unsigned long long)process->pid, (void *)(uintptr_t)process->space.vm.root,
             (unsigned long long)frame->cs, (unsigned long long)frame->ss,
             (void *)(uintptr_t)top, (void *)frame);
    }
    uint64_t number = frame->rax;
    if (number < SYS_COUNT) { ++process->calls[number]; }
    long result = SYS_ENOSYS;
    if (number == SYS_GETPID) { result = (long)process->pid; }
    else if (number == SYS_YIELD) {
        frame->rax = 0;
        return scheduler_interrupt(frame, false);
    } else if (number == SYS_EXIT) {
        int64_t status = (int64_t)frame->rdi;
        if (status < INT_MIN || status > INT_MAX) { result = SYS_EINVAL; }
        else { return task_finish_user(frame, (int)status); }
    } else if ((number == SYS_WRITE || number == SYS_READ) && frame->rdi >= 3) {
        result = fs_syscall(process, number, frame->rdi, frame->rsi, frame->rdx);
    } else if (number == SYS_WRITE || number == SYS_READ) {
        uint64_t fd = frame->rdi, address = frame->rsi, count = frame->rdx;
        bool write = number == SYS_WRITE;
        if ((write && fd != 1 && fd != 2) || (!write && fd != 0)) { result = SYS_EBADF; }
        else if (count > SYS_IO_MAX) { result = SYS_E2BIG; }
        else if (!user_buffer_valid(&process->space.vm, address, (size_t)count, !write)) { result = SYS_EFAULT; }
        else if (count == 0) { result = 0; }
        else {
            char buffer[SYS_IO_MAX + 1];
            if (write) {
                KASSERT(copy_from_user(&process->space.vm, buffer, address, (size_t)count));
                for (size_t i = 0; i < (size_t)count; ++i) {
                    if (process->terminal && (buffer[i] == '\n' || buffer[i] == '\r' || buffer[i] == '\t' || buffer[i] == '\b')) { continue; }
                    if ((unsigned char)buffer[i] < 32 || (unsigned char)buffer[i] > 126) { buffer[i] = '?'; }
                }
                buffer[count] = '\0';
                if (process->terminal) { serial_write(buffer); }
                else { klog(LOG_INFO, "user-output", "pid=%llu fd=%llu text=\"%s\" bytes=%u",
                     (unsigned long long)process->pid, (unsigned long long)fd, buffer, (unsigned int)count);
                }
                result = (long)count;
            } else {
                size_t received = 0;
                while (received < (size_t)count && serial_try_read(&buffer[received])) { ++received; }
                KASSERT(copy_to_user(&process->space.vm, address, buffer, received));
                result = (long)received;
            }
        }
    } else if (number >= SYS_SYSTEM && number <= SYS_SLEEP) { result = system_syscall(process, number, frame->rdi, frame->rsi, frame->rdx); }
    else { result = fs_syscall(process, number, frame->rdi, frame->rsi, frame->rdx); }
    if (result < 0) { ++process->rejected_calls; }
    frame->rax = (uint64_t)result;
    return user_return(frame);
}
