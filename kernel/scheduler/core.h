#ifndef AGENTICOS_SCHEDULER_CORE_H
#define AGENTICOS_SCHEDULER_CORE_H
#include <stddef.h>
#include "process/process.h"
#define TASK_CAPACITY 16u
#define TASK_NONE UINT64_MAX
enum task_state { TASK_UNUSED, TASK_READY, TASK_RUNNING, TASK_BLOCKED, TASK_ZOMBIE };
enum task_wait { WAIT_NONE, WAIT_SLEEP, WAIT_JOIN, WAIT_MUTEX, WAIT_SEMAPHORE };
struct task_control {
    uint64_t tid, pid, runtime_ticks, dispatches, wake_tick, wait_order, wait_tid, joiner;
    uintptr_t context;
    const void *wait_object;
    enum task_state state;
    enum task_wait wait;
    int exit_status;
    unsigned int held_mutexes;
};
/* Pure single-CPU policy. Callers serialize mutations; it never frees stacks. */
struct scheduler_core {
    struct process_table processes;
    struct task_control tasks[TASK_CAPACITY];
    uint64_t next_tid, next_wait, ticks, switches, preemptions;
    unsigned int current, live_tasks, quantum, slice;
    bool preemptive, ready;
};
bool sched_core_init(struct scheduler_core *core, uint64_t root);
struct task_control *sched_core_find(struct scheduler_core *core, uint64_t tid);
unsigned int sched_core_slot(const struct scheduler_core *core);
bool sched_core_add(struct scheduler_core *core, unsigned int slot, uint64_t pid, uintptr_t context, uint64_t *tid);
bool sched_core_block(struct scheduler_core *core, enum task_wait reason, const void *object, uint64_t target);
bool sched_core_sleep(struct scheduler_core *core, uint64_t delay);
uint64_t sched_core_wake_one(struct scheduler_core *core, enum task_wait reason, const void *object, uint64_t target);
bool sched_core_finish(struct scheduler_core *core, uint64_t tid, int status);
bool sched_core_reap(struct scheduler_core *core, uint64_t tid);
uintptr_t sched_core_schedule(struct scheduler_core *core, uintptr_t saved);
uintptr_t sched_core_tick(struct scheduler_core *core, uintptr_t saved);
#endif
