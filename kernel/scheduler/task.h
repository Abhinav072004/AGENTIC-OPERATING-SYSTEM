#ifndef AGENTICOS_TASK_H
#define AGENTICOS_TASK_H
#include "core.h"
#include "arch/x86_64/interrupts.h"
#define TASK_CANCELLED (-125)
typedef void (*task_entry)(void *argument);
struct task_mutex { uint64_t owner; bool ready; };
struct task_semaphore { unsigned int count, limit; bool ready; };
struct scheduler_stats { uint64_t ticks, switches, preemptions, idle_sleeps; unsigned int live_tasks; };
void scheduler_init(void);
bool task_process_create(const char *name, uint64_t *pid);
bool task_process_destroy(uint64_t pid);
bool task_spawn(uint64_t pid, task_entry entry, void *argument, uint64_t *tid);
uint64_t task_current(void);
uint64_t task_pid(void);
uint64_t task_ticks(void);
bool task_get_info(uint64_t tid, struct task_control *info);
void task_yield(void);
bool task_sleep(uint64_t delay);
_Noreturn void task_exit(int status);
bool task_cancel(uint64_t tid);
bool task_join(uint64_t tid, int *status);
void scheduler_run(bool preemptive, unsigned int quantum, uint64_t watchdog_ticks);
void scheduler_get_stats(struct scheduler_stats *stats);
struct interrupt_frame *scheduler_interrupt(struct interrupt_frame *frame, bool timer);
void task_mutex_init(struct task_mutex *mutex);
bool task_mutex_lock(struct task_mutex *mutex);
bool task_mutex_unlock(struct task_mutex *mutex);
bool task_semaphore_init(struct task_semaphore *semaphore, unsigned int initial, unsigned int limit);
bool task_semaphore_wait(struct task_semaphore *semaphore);
bool task_semaphore_post(struct task_semaphore *semaphore);
void scheduler_self_test(void);
#endif
