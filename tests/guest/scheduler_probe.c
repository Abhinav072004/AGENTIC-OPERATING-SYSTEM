/* Deliberate Week 5 failures; excluded from the normal kernel. */
#include "scheduler/task.h"
#include "scheduler/stack.h"
#include "debug/panic.h"
_Noreturn void scheduler_failure_probe(unsigned int mode);
static void guard_worker(void *argument) {
    (void)argument;
    volatile unsigned char value = *(volatile unsigned char *)(uintptr_t)(TASK_STACK_BASE + TASK_STACK_STRIDE);
    (void)value;
    PANIC("task guard unexpectedly mapped");
}
static void runaway_worker(void *argument) {
    (void)argument;
    for (;;) { __asm__ volatile ("pause" : : : "memory"); }
}
_Noreturn void scheduler_failure_probe(unsigned int mode) {
    uint64_t tid;
    KASSERT(task_spawn(KERNEL_PID, mode == 17 ? guard_worker : runaway_worker, NULL, &tid));
    scheduler_run(true, 2, 8);
    PANIC("scheduler failure probe unexpectedly completed");
}
