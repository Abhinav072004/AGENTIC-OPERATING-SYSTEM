#include "core.h"
#include "runtime/memory.h"

bool sched_core_init(struct scheduler_core *core, uint64_t root) {
    if (core == NULL) { return false; }
    memset(core, 0, sizeof(*core));
    if (!process_table_init(&core->processes, root) || !process_retain_thread(&core->processes, KERNEL_PID)) { return false; }
    core->tasks[0] = (struct task_control){.tid = 0, .pid = KERNEL_PID, .state = TASK_RUNNING, .joiner = TASK_NONE};
    core->next_tid = 1;
    core->next_wait = 1;
    core->quantum = 2;
    core->ready = true;
    return true;
}
struct task_control *sched_core_find(struct scheduler_core *core, uint64_t tid) {
    if (core == NULL || !core->ready || tid == TASK_NONE) { return NULL; }
    for (size_t i = 0; i < TASK_CAPACITY; ++i) {
        if (core->tasks[i].state != TASK_UNUSED && core->tasks[i].tid == tid) { return &core->tasks[i]; }
    }
    return NULL;
}
unsigned int sched_core_slot(const struct scheduler_core *core) {
    if (core == NULL || !core->ready) { return TASK_CAPACITY; }
    for (unsigned int i = 1; i < TASK_CAPACITY; ++i) { if (core->tasks[i].state == TASK_UNUSED) { return i; } }
    return TASK_CAPACITY;
}
bool sched_core_add(struct scheduler_core *core, unsigned int slot, uint64_t pid, uintptr_t context, uint64_t *tid) {
    if (core == NULL || !core->ready || slot == 0 || slot >= TASK_CAPACITY || context == 0 || tid == NULL ||
        core->tasks[slot].state != TASK_UNUSED || core->next_tid == TASK_NONE ||
        !process_retain_thread(&core->processes, pid)) { return false; }
    core->tasks[slot] = (struct task_control){.tid = core->next_tid++, .pid = pid,
        .context = context, .state = TASK_READY, .joiner = TASK_NONE};
    ++core->live_tasks;
    *tid = core->tasks[slot].tid;
    return true;
}
bool sched_core_block(struct scheduler_core *core, enum task_wait reason, const void *object, uint64_t target) {
    if (core == NULL || !core->ready || core->current == 0 || core->current >= TASK_CAPACITY ||
        reason < WAIT_SLEEP || reason > WAIT_SEMAPHORE || core->next_wait == UINT64_MAX) { return false; }
    struct task_control *task = &core->tasks[core->current];
    if (task->state != TASK_RUNNING) { return false; }
    task->state = TASK_BLOCKED;
    task->wait = reason;
    task->wait_object = object;
    task->wait_tid = target;
    task->wait_order = core->next_wait++;
    return true;
}
bool sched_core_sleep(struct scheduler_core *core, uint64_t delay) {
    if (core == NULL || delay == 0 || delay > UINT64_MAX - core->ticks ||
        !sched_core_block(core, WAIT_SLEEP, NULL, 0)) { return false; }
    core->tasks[core->current].wake_tick = core->ticks + delay;
    return true;
}
static void make_ready(struct task_control *task) {
    task->state = TASK_READY;
    task->wait = WAIT_NONE;
    task->wait_object = NULL;
    task->wait_tid = 0;
    task->wake_tick = 0;
    task->wait_order = 0;
}
uint64_t sched_core_wake_one(struct scheduler_core *core, enum task_wait reason, const void *object, uint64_t target) {
    if (core == NULL || !core->ready || reason < WAIT_JOIN || reason > WAIT_SEMAPHORE) { return TASK_NONE; }
    struct task_control *oldest = NULL;
    for (size_t i = 1; i < TASK_CAPACITY; ++i) {
        struct task_control *task = &core->tasks[i];
        if (task->state == TASK_BLOCKED && task->wait == reason && task->wait_object == object &&
            (reason != WAIT_JOIN || task->wait_tid == target) &&
            (oldest == NULL || task->wait_order < oldest->wait_order)) { oldest = task; }
    }
    if (oldest == NULL) { return TASK_NONE; }
    make_ready(oldest);
    return oldest->tid;
}
bool sched_core_finish(struct scheduler_core *core, uint64_t tid, int status) {
    struct task_control *task = sched_core_find(core, tid);
    if (task == NULL || tid == 0 || task->state == TASK_ZOMBIE || task->held_mutexes != 0) { return false; }
    /* A joiner can be cancelled after wakeup but before consuming the result. */
    for (size_t i = 1; i < TASK_CAPACITY; ++i) {
        if (core->tasks[i].joiner == tid) { core->tasks[i].joiner = TASK_NONE; }
    }
    task->state = TASK_ZOMBIE;
    task->wait = WAIT_NONE;
    task->exit_status = status;
    --core->live_tasks;
    while (sched_core_wake_one(core, WAIT_JOIN, NULL, tid) != TASK_NONE) { }
    return true;
}
bool sched_core_reap(struct scheduler_core *core, uint64_t tid) {
    struct task_control *task = sched_core_find(core, tid);
    if (task == NULL || tid == 0 || task->state != TASK_ZOMBIE ||
        task == &core->tasks[core->current] || !process_release_thread(&core->processes, task->pid)) { return false; }
    memset(task, 0, sizeof(*task));
    return true;
}
uintptr_t sched_core_schedule(struct scheduler_core *core, uintptr_t saved) {
    if (core == NULL || !core->ready || core->current >= TASK_CAPACITY || saved == 0) { return 0; }
    unsigned int old = core->current, next = 0;
    struct task_control *previous = &core->tasks[old];
    previous->context = saved;
    if (previous->state == TASK_RUNNING) { previous->state = TASK_READY; }
    for (unsigned int step = 1; step <= TASK_CAPACITY; ++step) {
        unsigned int slot = (old + step) % TASK_CAPACITY;
        if (slot != 0 && core->tasks[slot].state == TASK_READY) { next = slot; break; }
    }
    core->current = next;
    core->tasks[next].state = TASK_RUNNING;
    core->slice = 0;
    if (next != old) { ++core->switches; ++core->tasks[next].dispatches; }
    return core->tasks[next].context;
}
uintptr_t sched_core_tick(struct scheduler_core *core, uintptr_t saved) {
    if (core == NULL || !core->ready || saved == 0 || core->ticks == UINT64_MAX ||
        core->current >= TASK_CAPACITY || core->tasks[core->current].state != TASK_RUNNING) { return 0; }
    ++core->ticks;
    for (size_t i = 1; i < TASK_CAPACITY; ++i) {
        struct task_control *task = &core->tasks[i];
        if (task->state == TASK_BLOCKED && task->wait == WAIT_SLEEP && task->wake_tick <= core->ticks) { make_ready(task); }
    }
    unsigned int old = core->current;
    core->tasks[old].context = saved;
    ++core->tasks[old].runtime_ticks;
    if (old == 0 || (core->preemptive && ++core->slice >= core->quantum)) {
        uintptr_t next = sched_core_schedule(core, saved);
        if (old != 0 && core->current != old) { ++core->preemptions; }
        return next;
    }
    return saved;
}
