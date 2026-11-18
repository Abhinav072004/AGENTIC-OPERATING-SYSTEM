#include "system.h"
#include "user/copy.h"
#include "scheduler/task.h"
#include "memory/init.h"
#include "interrupts/interrupts.h"
#include "runtime/memory.h"
long system_syscall(struct user_process *process, uint64_t number, uint64_t a, uint64_t b, uint64_t c) {
    (void)c;
    if (number == SYS_SYSTEM) {
        struct system_info info = {.ticks = interrupts_ticks(), .free_frames = memory_frames()->free_frames,
            .managed_frames = memory_frames()->managed_frames, .fs_blocks = fs_used_blocks()};
        const char version[] = AGENTICOS_VERSION; memcpy(info.version, version, sizeof(version));
        return copy_to_user(&process->space.vm, a, &info, sizeof(info)) ? 0 : SYS_EFAULT;
    }
    if (number == SYS_TASK) {
        if (a >= TASK_CAPACITY) { return FS_EINVAL; }
        struct task_info info;
        if (!user_buffer_valid(&process->space.vm, b, sizeof(info), true)) { return SYS_EFAULT; }
        if (!task_snapshot((unsigned int)a, &info)) { return 0; }
        return copy_to_user(&process->space.vm, b, &info, sizeof(info)) ? 1 : SYS_EFAULT;
    }
    if (number == SYS_SLEEP) { return a == 0 || a > 60000 ? FS_EINVAL : (task_sleep(a) ? 0 : FS_EINVAL); }
    if (number == SYS_SPAWN) {
        char path[FS_PATH_MAX];
        for (size_t i = 0; i < sizeof(path); ++i) {
            if (a > UINT64_MAX - i || !copy_from_user(&process->space.vm, &path[i], a + i, 1)) { return SYS_EFAULT; }
            if (path[i] == '\0') { return user_spawn_path(path, b, process->pid, process->terminal); }
        }
        return FS_ENAMETOOLONG;
    }
    if (number == SYS_WAIT || number == SYS_KILL) {
        struct user_process *child = user_find(a);
        if (child == NULL) { return SYS_ESRCH; }
        if (child->parent != process->pid) { return SYS_EPERM; }
        if (number == SYS_KILL) { return task_cancel(child->tid) ? 0 : SYS_EINVAL; }
        if (!user_buffer_valid(&process->space.vm, b, sizeof(int), true)) { return SYS_EFAULT; }
        struct task_control task; int status;
        if (!task_get_info(child->tid, &task) || task.state != TASK_ZOMBIE) { return SYS_EAGAIN; }
        if (!user_process_reap(child->pid, &status)) { return SYS_EINVAL; }
        return copy_to_user(&process->space.vm, b, &status, sizeof(status)) ? 0 : SYS_EFAULT;
    }
    return SYS_ENOSYS;
}
