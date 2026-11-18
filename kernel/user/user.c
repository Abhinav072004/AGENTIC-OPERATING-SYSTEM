#include "user.h"
#include "scheduler/task.h"
#include "memory/init.h"
#include "arch/x86_64/memory.h"
#include "arch/x86_64/io.h"
#include "syscall/abi.h"
#include "debug/log.h"
#include "debug/panic.h"
#include "runtime/memory.h"
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
    if (pid == NULL || tid == NULL || process == NULL || !memory_address_space()->active ||
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
    process->pid = new_pid; process->tid = new_tid; process->name = name; process->live = true;
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
    fs_close_all(&process->files);
    KASSERT(user_space_destroy(&process->space));
    KASSERT(task_reap_user(process->tid, status) && task_process_destroy(pid));
    memset(process, 0, sizeof(*process));
    irq_restore(flags);
    return true;
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
