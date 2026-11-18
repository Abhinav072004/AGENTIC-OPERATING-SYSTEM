#include "task.h"
#include "stack.h"
#include "memory/init.h"
#include "interrupts/interrupts.h"
#include "arch/x86_64/io.h"
#include "debug/panic.h"
#include "runtime/memory.h"
#include "user/user.h"

#define STACK_CANARY UINT64_C(0x47ab89cde123650f)
static struct scheduler_core core;
struct task_storage { struct task_stack stack; task_entry entry; void *argument; bool user; };
static struct task_storage storage[TASK_CAPACITY];
static volatile bool active;
static uint64_t idle_sleeps, run_start, watchdog;
static bool allocate(void *context, uint64_t *physical) { (void)context; return pmm_alloc_frame(memory_frames(), physical); }
static bool release(void *context, uint64_t physical) {
    (void)context;
    KASSERT(pmm_free_frame(memory_frames(), physical));
    return true;
}
static bool mapped(void *context, uint64_t address) { (void)context; return vmm_query(memory_address_space(), address, NULL, NULL); }
static bool map(void *context, uint64_t address, uint64_t physical) {
    (void)context;
    return vmm_map(memory_address_space(), address, physical, VM_WRITE | VM_NX);
}
static bool unmap(void *context, uint64_t address, uint64_t *physical) {
    (void)context;
    KASSERT(vmm_unmap(memory_address_space(), address, physical));
    return true;
}
static const struct task_stack_ops stack_ops = {NULL, allocate, release, mapped, map, unmap};
static void switch_now(void) { __asm__ volatile ("int $0x81" : : : "memory"); }
static struct task_control *current(void) { return &core.tasks[core.current]; }
static void check_stack(unsigned int slot, const struct interrupt_frame *frame) {
    if (slot == 0) { return; }
    const struct task_stack *stack = &storage[slot].stack;
    KASSERT(stack->ready && *(volatile uint64_t *)(uintptr_t)stack->bottom == STACK_CANARY);
    KASSERT((uintptr_t)frame >= stack->bottom + 16 && (uintptr_t)frame <= stack->top - sizeof(*frame));
    if ((frame->cs & 3u) == 0) { KASSERT(frame->rsp >= stack->bottom + 16 && frame->rsp < stack->top); }
    else { KASSERT(storage[slot].user); } /* User RSP is untrusted; validate before IRET. */
}
static _Noreturn void task_start(void) {
    unsigned int slot = core.current;
    KASSERT(active && slot != 0 && storage[slot].entry != NULL);
    storage[slot].entry(storage[slot].argument);
    task_exit(0);
}
void scheduler_init(void) {
    uint64_t flags = irq_save();
    KASSERT(!core.ready && memory_address_space()->active);
    KASSERT(sched_core_init(&core, memory_address_space()->root));
    irq_restore(flags);
}
bool task_process_create(const char *name, uint64_t *pid) {
    uint64_t flags = irq_save();
    bool result = process_create(&core.processes, name, pid);
    irq_restore(flags);
    return result;
}
bool task_process_destroy(uint64_t pid) {
    uint64_t flags = irq_save();
    bool result = process_destroy(&core.processes, pid);
    irq_restore(flags);
    return result;
}
bool task_spawn(uint64_t pid, task_entry entry, void *argument, uint64_t *tid) {
    uint64_t flags = irq_save();
    unsigned int slot = sched_core_slot(&core);
    struct process_control *process = process_find(&core.processes, pid);
    if (entry == NULL || tid == NULL || slot == TASK_CAPACITY || process == NULL || process->isolated ||
        !task_stack_create(&storage[slot].stack, slot, &stack_ops)) { irq_restore(flags); return false; }
    struct task_stack *stack = &storage[slot].stack;
    memset((void *)(uintptr_t)stack->bottom, 0, TASK_STACK_BYTES);
    *(uint64_t *)(uintptr_t)stack->bottom = STACK_CANARY;
    struct interrupt_frame *frame = (void *)(uintptr_t)(stack->top - 192);
    *frame = (struct interrupt_frame){.rip = (uintptr_t)task_start, .cs = X86_KERNEL_CODE,
        .ss = X86_KERNEL_DATA, .rflags = 0x202, .rsp = stack->top - 8, .vector = 129};
    if (!sched_core_add(&core, slot, pid, (uintptr_t)frame, tid)) {
        KASSERT(task_stack_destroy(stack));
        irq_restore(flags);
        return false;
    }
    storage[slot].entry = entry;
    storage[slot].argument = argument;
    storage[slot].user = false;
    irq_restore(flags);
    return true;
}
uint64_t task_current(void) { uint64_t flags = irq_save(), tid = current()->tid; irq_restore(flags); return tid; }
uint64_t task_pid(void) { uint64_t flags = irq_save(), pid = current()->pid; irq_restore(flags); return pid; }
uint64_t task_ticks(void) { uint64_t flags = irq_save(), ticks = core.ticks; irq_restore(flags); return ticks; }
bool task_get_info(uint64_t tid, struct task_control *info) {
    uint64_t flags = irq_save();
    struct task_control *task = sched_core_find(&core, tid);
    bool result = task != NULL && info != NULL;
    if (result) { *info = *task; }
    irq_restore(flags);
    return result;
}
bool task_snapshot(unsigned int slot, struct task_info *info) {
    if (slot >= TASK_CAPACITY || info == NULL || core.tasks[slot].state == TASK_UNUSED) { return false; }
    const struct task_control *task = &core.tasks[slot];
    const struct process_control *process = process_find(&core.processes, task->pid);
    KASSERT(process != NULL);
    *info = (struct task_info){.pid = task->pid, .tid = task->tid, .runtime_ticks = task->runtime_ticks, .state = (uint32_t)task->state, .isolated = process->isolated ? 1u : 0u};
    memcpy(info->name, process->name, sizeof(info->name)); return true;
}
void task_yield(void) { KASSERT(active); switch_now(); }
bool task_sleep(uint64_t delay) {
    uint64_t flags = irq_save();
    bool result = active && sched_core_sleep(&core, delay);
    if (result) { switch_now(); }
    irq_restore(flags);
    return result;
}
_Noreturn void task_exit(int status) {
    (void)irq_save();
    KASSERT(active && sched_core_finish(&core, current()->tid, status));
    switch_now();
    PANIC("exited task resumed");
}
bool task_cancel(uint64_t tid) {
    uint64_t flags = irq_save();
    struct task_control *task = sched_core_find(&core, tid);
    if (task != NULL && tid != current()->tid && task->state != TASK_ZOMBIE && task->held_mutexes == 0 && storage[(size_t)(task-core.tasks)].user) { user_cancel_children(task->pid); }
    bool result = task != NULL && tid != current()->tid && sched_core_finish(&core, tid, TASK_CANCELLED);
    irq_restore(flags);
    return result;
}
bool task_join(uint64_t tid, int *status) {
    uint64_t flags = irq_save(), caller = current()->tid;
    struct task_control *task = sched_core_find(&core, tid);
    if (task == NULL || tid == 0 || storage[(size_t)(task - core.tasks)].user || tid == caller || (task->joiner != TASK_NONE && task->joiner != caller) ||
        (task->state != TASK_ZOMBIE && (!active || core.current == 0))) { irq_restore(flags); return false; }
    /* Reject cycles before reserving a join or blocking. */
    struct task_control *link = task;
    for (size_t i = 0; i < TASK_CAPACITY && link->state == TASK_BLOCKED && link->wait == WAIT_JOIN; ++i) {
        if (link->wait_tid == caller) { irq_restore(flags); return false; }
        link = sched_core_find(&core, link->wait_tid);
        if (link == NULL) { irq_restore(flags); return false; }
    }
    task->joiner = caller;
    while (task->state != TASK_ZOMBIE) {
        KASSERT(sched_core_block(&core, WAIT_JOIN, NULL, tid));
        switch_now();
    }
    unsigned int slot = (unsigned int)(task - core.tasks);
    if (status != NULL) { *status = task->exit_status; }
    KASSERT(task_stack_destroy(&storage[slot].stack));
    storage[slot].entry = NULL;
    storage[slot].argument = NULL;
    KASSERT(sched_core_reap(&core, tid));
    irq_restore(flags);
    return true;
}
struct interrupt_frame *scheduler_interrupt(struct interrupt_frame *frame, bool timer) {
    if (!active) { KASSERT(timer); return frame; }
    check_stack(core.current, frame);
    uintptr_t next = timer ? sched_core_tick(&core, (uintptr_t)frame) : sched_core_schedule(&core, (uintptr_t)frame);
    KASSERT(next != 0);
    if (core.ticks - run_start > watchdog) { PANIC("scheduler watchdog expired"); }
    while ((((const struct interrupt_frame *)next)->cs & 3u) == 3 && !user_frame_prepare((struct interrupt_frame *)next)) {
        user_bad_context((const struct interrupt_frame *)next);
        KASSERT(sched_core_finish(&core, current()->tid, USER_BAD_CONTEXT));
        next = sched_core_schedule(&core, next);
        KASSERT(next != 0);
    }
    check_stack(core.current, (const struct interrupt_frame *)next);
    extern unsigned char stack_top[];
    arch_interrupts_kernel_stack(core.current == 0 ? (uintptr_t)stack_top : storage[core.current].stack.top);
    struct process_control *process = process_find(&core.processes, current()->pid);
    KASSERT(process != NULL && user_switch_root(process->address_space));
    return (struct interrupt_frame *)next;
}
static void user_unreachable(void *argument) { (void)argument; PANIC("user task entered kernel trampoline"); }
bool task_spawn_user(uint64_t pid, uint64_t entry, uint64_t stack, uint64_t argument, uint64_t *tid) {
    uint64_t flags = irq_save(), id;
    struct process_control *process = process_find(&core.processes, pid);
    if (tid == NULL || process == NULL || pid == KERNEL_PID || process->threads != 0 ||
        entry < USER_IMAGE_BASE || entry >= USER_IMAGE_END || stack < USER_STACK_BOTTOM || stack >= USER_STACK_TOP ||
        !task_spawn(pid, user_unreachable, NULL, &id)) { irq_restore(flags); return false; }
    struct task_control *task = sched_core_find(&core, id);
    struct interrupt_frame *frame = (void *)task->context;
    frame->rip = entry; frame->rsp = stack; frame->cs = X86_USER_CODE; frame->ss = X86_USER_DATA;
    frame->rdi = pid; frame->rsi = argument;
    storage[(size_t)(task - core.tasks)].user = true;
    *tid = id;
    irq_restore(flags); return true;
}
bool task_process_bind_user(uint64_t pid, uint64_t root) {
    uint64_t flags = irq_save();
    struct process_control *process = process_find(&core.processes, pid);
    bool result = process != NULL && pid != KERNEL_PID && process->threads == 1 &&
                  root % 4096 == 0 && root < (UINT64_C(1) << 52) && root != memory_address_space()->root;
    if (result) { process->address_space = root; process->isolated = true; }
    irq_restore(flags); return result;
}
bool task_reap_user(uint64_t tid, int *status) {
    uint64_t flags = irq_save();
    struct task_control *task = sched_core_find(&core, tid);
    if (task == NULL || task == current() || task->state != TASK_ZOMBIE ||
        !storage[(size_t)(task - core.tasks)].user) { irq_restore(flags); return false; }
    size_t slot = (size_t)(task - core.tasks);
    storage[slot].user = false;
    bool result = task_join(tid, status);
    if (!result) { storage[slot].user = true; }
    irq_restore(flags); return result;
}
bool task_is_user(void) { return active && core.current != 0 && storage[core.current].user; }
struct interrupt_frame *task_finish_user(struct interrupt_frame *frame, int status) {
    user_cancel_children(task_pid());
    KASSERT(task_is_user() && (frame->cs & 3u) == 3 && sched_core_finish(&core, current()->tid, status));
    return scheduler_interrupt(frame, false);
}
void scheduler_run(bool preemptive, unsigned int quantum, uint64_t watchdog_ticks) {
    uint64_t flags = irq_save();
    KASSERT(core.ready && !active && core.current == 0 && quantum > 0 && quantum <= 100 && watchdog_ticks > 0);
    KASSERT((flags & UINT64_C(0x200)) == 0);
    core.preemptive = preemptive;
    core.quantum = quantum;
    core.slice = 0;
    active = true;
    run_start = core.ticks;
    watchdog = watchdog_ticks;
    interrupts_timer_start();
    while (core.live_tasks != 0) {
        switch_now();
        if (core.live_tasks != 0) {
            ++idle_sleeps;
            __asm__ volatile ("sti; hlt; cli" : : : "memory");
        }
    }
    interrupts_timer_stop();
    active = false;
    KASSERT(core.current == 0);
    irq_restore(flags);
}
void scheduler_get_stats(struct scheduler_stats *stats) {
    uint64_t flags = irq_save();
    KASSERT(stats != NULL);
    *stats = (struct scheduler_stats){core.ticks, core.switches, core.preemptions, idle_sleeps, core.live_tasks};
    irq_restore(flags);
}
void task_mutex_init(struct task_mutex *mutex) {
    if (mutex != NULL) { *mutex = (struct task_mutex){.owner = TASK_NONE, .ready = true}; }
}
bool task_mutex_lock(struct task_mutex *mutex) {
    uint64_t flags = irq_save();
    if (!active || core.current == 0 || mutex == NULL || !mutex->ready || mutex->owner == current()->tid) {
        irq_restore(flags); return false;
    }
    if (mutex->owner != TASK_NONE) {
        KASSERT(sched_core_block(&core, WAIT_MUTEX, mutex, 0));
        switch_now();
        KASSERT(mutex->owner == current()->tid); /* Unlock transfers ownership before wakeup. */
    } else { mutex->owner = current()->tid; ++current()->held_mutexes; }
    irq_restore(flags);
    return true;
}
bool task_mutex_unlock(struct task_mutex *mutex) {
    uint64_t flags = irq_save();
    if (!active || mutex == NULL || !mutex->ready || mutex->owner != current()->tid) { irq_restore(flags); return false; }
    KASSERT(current()->held_mutexes > 0);
    --current()->held_mutexes;
    uint64_t next = sched_core_wake_one(&core, WAIT_MUTEX, mutex, 0);
    mutex->owner = next;
    if (next != TASK_NONE) { ++sched_core_find(&core, next)->held_mutexes; }
    irq_restore(flags);
    return true;
}
bool task_semaphore_init(struct task_semaphore *semaphore, unsigned int initial, unsigned int limit) {
    if (semaphore == NULL || limit == 0 || initial > limit) { return false; }
    *semaphore = (struct task_semaphore){initial, limit, true};
    return true;
}
bool task_semaphore_wait(struct task_semaphore *semaphore) {
    uint64_t flags = irq_save();
    if (!active || core.current == 0 || semaphore == NULL || !semaphore->ready) { irq_restore(flags); return false; }
    while (semaphore->count == 0) {
        KASSERT(sched_core_block(&core, WAIT_SEMAPHORE, semaphore, 0));
        switch_now();
    }
    --semaphore->count;
    irq_restore(flags);
    return true;
}
bool task_semaphore_post(struct task_semaphore *semaphore) {
    uint64_t flags = irq_save();
    if (semaphore == NULL || !semaphore->ready || semaphore->count == semaphore->limit) { irq_restore(flags); return false; }
    ++semaphore->count;
    /* Wake all: cancellation of a woken waiter cannot strand a posted token. */
    while (sched_core_wake_one(&core, WAIT_SEMAPHORE, semaphore, 0) != TASK_NONE) { }
    irq_restore(flags);
    return true;
}
