#include "user.h"
#include "scheduler/task.h"
#include "memory/init.h"
#include "arch/x86_64/memory.h"
#include "arch/x86_64/io.h"
#include "syscall/abi.h"
#include "debug/log.h"
#include "debug/panic.h"
#include "runtime/memory.h"
#include "net/stack.h"
#include "security/policy.h"
#include "agent/runtime.h"
static struct user_process processes[PROCESS_CAPACITY - 1u];
struct user_process *user_find(uint64_t pid) {
    for (size_t i = 0; i < PROCESS_CAPACITY - 1u; ++i) { if (processes[i].live && processes[i].pid == pid) { return &processes[i]; } }
    return NULL;
}
bool user_switch_root(uint64_t root) {
    struct vmm *next = root == memory_address_space()->root ? memory_address_space() : NULL;
    for (size_t i = 0; i < PROCESS_CAPACITY - 1u; ++i) {
        if (processes[i].live && processes[i].space.vm.root == root) { next = &processes[i].space.vm; }
    }
    if (next == NULL || !next->ready || !arch_memory_switch(root)) { return false; }
    memory_address_space()->active = false;
    for (size_t i = 0; i < PROCESS_CAPACITY - 1u; ++i) { processes[i].space.vm.active = false; }
    next->active = true;
    return true;
}
bool user_process_start(const void *image, size_t length, const char *name, uint64_t mode,
                        bool private_page, uint64_t *pid, uint64_t *tid) {
    uint64_t flags = irq_save();
    struct elf_plan plan;
    struct user_process *process = NULL;
    for (size_t i = 0; i < PROCESS_CAPACITY - 1u; ++i) { if (!processes[i].live) { process = &processes[i]; break; } }
    if (pid == NULL || tid == NULL || process == NULL ||
        !user_elf_plan(image, length, &plan)) { irq_restore(flags); return false; }
    uint64_t new_pid, new_tid;
    if (!task_process_create(name, &new_pid)) { irq_restore(flags); return false; }
    /* Allocate the kernel stack first: its upper-half subtree must exist
     * before a user root borrows it. All worker slots share this subtree. */
    if (!task_spawn_user(new_pid, plan.entry, USER_STACK_TOP - 16, mode, &new_tid)) {
        KASSERT(task_process_destroy(new_pid)); irq_restore(flags); return false;
    }
    memset(process, 0, sizeof(*process));
    if (!user_space_load(&process->space, memory_address_space(), image, length, new_pid, private_page)) {
        KASSERT(task_cancel(new_tid) && task_reap_user(new_tid, NULL) && task_process_destroy(new_pid));
        irq_restore(flags); return false;
    }
    process->uid = 1000; process->capabilities = CAP_SESSION;
    process->pid = new_pid; process->tid = new_tid; process->live = true;
    for (size_t i = 0; i + 1 < sizeof(process->name) && name[i] != '\0'; ++i) { process->name[i] = name[i]; }
    KASSERT(task_process_bind_user(new_pid, process->space.vm.root));
    *pid = new_pid; *tid = new_tid;
    irq_restore(flags);
    return true;
}
bool user_process_reap(uint64_t pid, int *status) {
    uint64_t flags = irq_save();
    struct user_process *process = user_find(pid);
    struct task_control task;
    if (process == NULL || !task_get_info(process->tid, &task) || task.state != TASK_ZOMBIE || process->space.vm.active) {
        irq_restore(flags); return false;
    }
    for (size_t i = 0; i < PROCESS_CAPACITY - 1u; ++i) { if (processes[i].live && processes[i].parent == pid) { KASSERT(user_process_reap(processes[i].pid, NULL)); } }
    net_release(process->pid);
    fs_close_all(&process->files);
    KASSERT(user_space_destroy(&process->space));
    int exit_status;
    KASSERT(task_reap_user(process->tid, &exit_status) && task_process_destroy(pid));
    if (status != NULL) { *status = exit_status; }
    agent_process_reaped(pid,exit_status);
    memset(process, 0, sizeof(*process));
    irq_restore(flags);
    return true;
}
/* Image I/O is scoped to the creator's UID. No scheduling occurs while the
 * RAM filesystem identity is changed; reset it before every exit path. */
long user_spawn_path(const char *path, uint64_t mode, uint64_t parent, bool terminal) {
    struct user_process *creator = user_find(parent);
    fs_set_uid(creator == NULL ? 0 : creator->uid);
    struct fs_info info; long result = fs_stat(path, &info);
    if (result == 0 && info.directory != 0) { result = FS_EISDIR; }
    if (result == 0 && (info.size == 0 || info.size > FS_FILE_MAX)) { result = FS_EINVAL; }
    if (result == 0 && creator != NULL && (info.mode & (creator->uid == info.uid ? 0100u : 0001u)) == 0) { result = FS_EACCES; }
    struct fs_handles files = {0}; void *image = NULL;
    if (result == 0) {
        long fd = fs_open(&files, path, FS_READ);
        if (fd < 0) { result = fd; }
        else { image = kmalloc((size_t)info.size); result = image == NULL ? FS_ENOSPC : fs_read(&files, (uint64_t)fd, image, (size_t)info.size); }
    }
    fs_close_all(&files); fs_set_uid(0);
    uint64_t pid, tid;
    bool loaded = image != NULL && result == (long)info.size && user_process_start(image, (size_t)info.size, path, mode, false, &pid, &tid);
    KASSERT(kfree(image));
    if (!loaded) { return result < 0 ? result : FS_EINVAL; }
    struct user_process *process = user_find(pid); KASSERT(process != NULL);
    process->parent = parent; process->terminal = terminal;
    if (creator != NULL) { process->uid = creator->uid; process->capabilities = creator->capabilities & ~CAP_DISPLAY; process->cpu_budget = creator->cpu_budget; process->syscall_budget = creator->syscall_budget; }
    if (creator != NULL && creator->display) { process->stdout_parent = parent; }
    return (long)pid;
}
void user_cancel_children(uint64_t pid) {
    for (size_t i = 0; i < PROCESS_CAPACITY - 1u; ++i) {
        if (!processes[i].live || processes[i].parent != pid) { continue; }
        struct task_control task;
        if (task_get_info(processes[i].tid, &task) && task.state != TASK_ZOMBIE) { (void)task_cancel(processes[i].tid); }
    }
}
bool user_budget_expired(void) {
    if (!task_is_user()) { return false; }
    struct user_process *process = user_find(task_pid());
    return process != NULL && process->cpu_budget != 0 && process->timer_entries >= process->cpu_budget;
}
void user_bad_context(const struct interrupt_frame *frame) {
    struct user_process *process = user_find(task_pid());
    KASSERT(process != NULL);
    klog(LOG_WARNING, "user-fault", "case=%s pid=%llu reason=context rip=%p rsp=%p; process terminated",
         process->name, (unsigned long long)process->pid, (void *)(uintptr_t)frame->rip, (void *)(uintptr_t)frame->rsp);
}
void user_timer_entry(const struct interrupt_frame *frame) {
    if ((frame->cs & 3u) != 3) { return; }
    struct user_process *process = user_find(task_pid());
    KASSERT(process != NULL && task_is_user());
    ++process->timer_entries;
    volatile struct user_info *info = process->space.vm.ops.pointer(process->space.vm.ops.context, process->space.info_physical);
    info->ticks = process->timer_entries;
}
struct interrupt_frame *user_return(struct interrupt_frame *frame) {
    if (user_frame_prepare(frame)) { return frame; }
    user_bad_context(frame);
    return task_finish_user(frame, USER_BAD_CONTEXT);
}
struct interrupt_frame *user_fault(struct interrupt_frame *frame) {
    struct user_process *process = user_find(task_pid());
    KASSERT(process != NULL && task_is_user());
    uint64_t cr2;
    __asm__ volatile ("mov %%cr2, %0" : "=r"(cr2));
    klog(LOG_WARNING, "user-fault", "case=%s pid=%llu vector=%llu error=0x%llx rip=%p cr2=%p cs=0x%llx; process terminated",
         process->name, (unsigned long long)process->pid, (unsigned long long)frame->vector,
         (unsigned long long)frame->error, (void *)(uintptr_t)frame->rip,
         (void *)(uintptr_t)cr2, (unsigned long long)frame->cs);
    return task_finish_user(frame, USER_FAULT_STATUS(frame->vector));
}
