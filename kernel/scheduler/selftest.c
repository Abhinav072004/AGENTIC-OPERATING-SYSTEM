#include "task.h"
#include "stack.h"
#include "memory/init.h"
#include "arch/x86_64/io.h"
#include "arch/x86_64/serial.h"
#include "debug/log.h"
#include "debug/panic.h"

volatile uint64_t scheduler_preempt_signal;
static uint64_t group_pid, victim_tid, cycle_a, cycle_b;
static unsigned int trace[6], trace_count;
static volatile uint64_t work[3];
static struct task_mutex mutex;
static struct task_semaphore semaphore;
static unsigned int protected_count, in_critical, consumed, joined_child, joined_cycle;

static void immediate(void *argument) { (void)argument; task_exit(23); }
static void capacity_test(void) {
    uint64_t pid, ids[TASK_CAPACITY - 1], initial = memory_frames()->free_frames;
    KASSERT(task_process_create("capacity", &pid));
    uint64_t unchanged = TASK_NONE;
    KASSERT(!task_spawn(0, immediate, NULL, &unchanged) && unchanged == TASK_NONE);
    KASSERT(!task_spawn(pid, NULL, NULL, &unchanged) && unchanged == TASK_NONE);
    for (unsigned int i = 0; i < TASK_CAPACITY - 1; ++i) { KASSERT(task_spawn(pid, immediate, NULL, &ids[i])); }
    uint64_t full_free = memory_frames()->free_frames;
    KASSERT(!task_spawn(pid, immediate, NULL, &unchanged) && unchanged == TASK_NONE);
    KASSERT(memory_frames()->free_frames == full_free && !task_process_destroy(pid));
    uint64_t physical, flags;
    KASSERT(!vmm_query(memory_address_space(), TASK_STACK_BASE + TASK_STACK_STRIDE, NULL, NULL));
    KASSERT(vmm_query(memory_address_space(), TASK_STACK_BASE + TASK_STACK_STRIDE + PAGE_SIZE, &physical, &flags));
    KASSERT(flags == (VM_WRITE | VM_NX));
    KASSERT(!vmm_query(memory_address_space(), TASK_STACK_BASE + TASK_STACK_STRIDE + PAGE_SIZE + TASK_STACK_BYTES, NULL, NULL));
    for (unsigned int i = 0; i < TASK_CAPACITY - 1; ++i) {
        int status;
        KASSERT(task_cancel(ids[i]) && task_join(ids[i], &status) && status == TASK_CANCELLED);
        KASSERT(!task_cancel(ids[i]) && !task_join(ids[i], NULL));
    }
    KASSERT(task_process_destroy(pid) && memory_frames()->free_frames == initial);
    klog(LOG_INFO, "scheduler-test", "capacity=15; invalid/capacity failures, cancellation and stack guards passed");
}
static void cooperative_worker(void *argument) {
    unsigned int index = (unsigned int)(uintptr_t)argument;
    volatile uint64_t local[16];
    for (unsigned int i = 0; i < 16; ++i) { local[i] = UINT64_C(0xabcde000) + index * 16u + i; }
    for (unsigned int i = 0; i < 3; ++i) {
        KASSERT(trace_count < 6);
        trace[trace_count++] = index;
        KASSERT(arch_task_probe(false));
        for (unsigned int j = 0; j < 16; ++j) { KASSERT(local[j] == UINT64_C(0xabcde000) + index * 16u + j); }
    }
    task_exit(7 + (int)index * 2);
}
static void cooperative_test(unsigned int round) {
    uint64_t pids[2], ids[2];
    trace_count = 0;
    for (unsigned int i = 0; i < 2; ++i) {
        KASSERT(task_process_create(i == 0 ? "cooperative-a" : "cooperative-b", &pids[i]));
        KASSERT(task_spawn(pids[i], cooperative_worker, (void *)(uintptr_t)i, &ids[i]));
    }
    scheduler_run(false, 2, 500);
    KASSERT(trace_count == 6);
    for (unsigned int i = 0; i < 6; ++i) { KASSERT(trace[i] == i % 2); }
    for (unsigned int i = 0; i < 2; ++i) {
        struct task_control info;
        int status;
        KASSERT(task_get_info(ids[i], &info) && info.state == TASK_ZOMBIE && info.pid == pids[i]);
        KASSERT(task_join(ids[i], &status) && status == 7 + (int)i * 2 && task_process_destroy(pids[i]));
    }
    klog(LOG_INFO, "scheduler-test", "cooperative round=%u order=ABABAB; 15 GPRs/RSP/CF/DF, separate stacks and exit status passed", round);
    serial_write("AGENTICOS: COOPERATIVE PASS\n");
}
static void preemptive_worker(void *argument) {
    unsigned int index = (unsigned int)(uintptr_t)argument;
    KASSERT(task_pid() == group_pid);
    uint64_t started = task_ticks();
    if (index == 0) { KASSERT(arch_task_probe(true)); }
    if (index == 1) { scheduler_preempt_signal = 1; }
    while (task_ticks() - started < 8) {
        ++work[index];
        __asm__ volatile ("pause" : : : "memory");
    }
}
static void preemptive_test(unsigned int round) {
    uint64_t ids[3];
    scheduler_preempt_signal = 0;
    for (unsigned int i = 0; i < 3; ++i) { work[i] = 0; }
    KASSERT(task_process_create("preemptive", &group_pid));
    for (unsigned int i = 0; i < 3; ++i) { KASSERT(task_spawn(group_pid, preemptive_worker, (void *)(uintptr_t)i, &ids[i])); }
    struct scheduler_stats before, after;
    scheduler_get_stats(&before);
    scheduler_run(true, CONFIG_SELFTEST == 16 ? 1 : 2, 500);
    scheduler_get_stats(&after);
    KASSERT(after.preemptions - before.preemptions >= 3);
    for (unsigned int i = 0; i < 3; ++i) {
        struct task_control info;
        int status;
        KASSERT(work[i] > 0 && task_get_info(ids[i], &info) && info.runtime_ticks > 0 && info.dispatches > 0);
        KASSERT(task_join(ids[i], &status) && status == 0);
    }
    KASSERT(task_process_destroy(group_pid));
    klog(LOG_INFO, "scheduler-test", "preemptive round=%u workers=3 timer_switches=%llu quantum=%u; non-yielding progress and 15 GPRs/RSP/DF passed",
         round, (unsigned long long)(after.preemptions - before.preemptions), CONFIG_SELFTEST == 16 ? 1u : 2u);
    serial_write("AGENTICOS: PREEMPT PASS\n");
}
static void mutex_worker(void *argument) {
    if ((uintptr_t)argument == 1) {
        KASSERT(!task_mutex_unlock(&mutex));
        if (mutex.owner != TASK_NONE) { KASSERT(!task_cancel(mutex.owner)); }
    }
    for (unsigned int i = 0; i < 4; ++i) {
        KASSERT(task_mutex_lock(&mutex) && !task_mutex_lock(&mutex));
        KASSERT(in_critical++ == 0);
        unsigned int value = protected_count;
        task_yield(); /* Another task must block while this owner retains the lock. */
        KASSERT(in_critical == 1);
        protected_count = value + 1;
        --in_critical;
        KASSERT(task_mutex_unlock(&mutex));
    }
}
static void consumer(void *argument) {
    (void)argument;
    for (unsigned int i = 0; i < 2; ++i) { KASSERT(task_semaphore_wait(&semaphore)); ++consumed; }
}
static void producer(void *argument) {
    (void)argument;
    KASSERT(task_sleep(2));
    uint64_t flags = irq_save();
    KASSERT(task_semaphore_post(&semaphore) && task_semaphore_post(&semaphore));
    KASSERT(!task_semaphore_post(&semaphore));
    irq_restore(flags);
}
static void sleeper(void *argument) {
    (void)argument;
    uint64_t start = task_ticks();
    KASSERT(task_sleep(5) && task_ticks() - start >= 5);
}
static void victim(void *argument) {
    (void)argument;
    KASSERT(task_sleep(100));
    PANIC("cancelled sleeper resumed");
}
static void coordinator(void *argument) {
    (void)argument;
    struct task_control info;
    do { KASSERT(task_get_info(victim_tid, &info)); if (info.state != TASK_BLOCKED) { task_yield(); } }
    while (info.state != TASK_BLOCKED);
    KASSERT(task_cancel(victim_tid));
    int status;
    KASSERT(task_join(victim_tid, &status) && status == TASK_CANCELLED);
    KASSERT(!task_get_info(victim_tid, &info));
    uint64_t child;
    KASSERT(task_spawn(group_pid, immediate, NULL, &child) && child != victim_tid);
    KASSERT(!task_cancel(victim_tid));
    KASSERT(task_join(child, &status) && status == 23); /* Blocks until child exits. */
    KASSERT(!task_join(child, NULL) && !task_join(task_current(), NULL));
    joined_child = 1;
}
static void cycle_first(void *argument) {
    (void)argument;
    int status;
    KASSERT(task_join(cycle_b, &status) && status == 37);
    joined_cycle = 1;
}
static void cycle_second(void *argument) {
    (void)argument;
    struct task_control first;
    KASSERT(task_get_info(cycle_a, &first) && first.state == TASK_BLOCKED && first.wait == WAIT_JOIN);
    KASSERT(!task_join(cycle_a, NULL));
    task_exit(37);
}
static void synchronization_test(unsigned int round) {
    uint64_t ids[9];
    protected_count = 0; in_critical = 0; consumed = 0; joined_child = 0; joined_cycle = 0;
    task_mutex_init(&mutex);
    KASSERT(task_semaphore_init(&semaphore, 0, 2));
    KASSERT(task_process_create("synchronization", &group_pid));
    KASSERT(task_spawn(group_pid, mutex_worker, NULL, &ids[0]));
    KASSERT(task_spawn(group_pid, mutex_worker, (void *)(uintptr_t)1, &ids[1]));
    KASSERT(task_spawn(group_pid, consumer, NULL, &ids[2]));
    KASSERT(task_spawn(group_pid, producer, NULL, &ids[3]));
    KASSERT(task_spawn(group_pid, sleeper, NULL, &ids[4]));
    KASSERT(task_spawn(group_pid, victim, NULL, &ids[5]));
    victim_tid = ids[5];
    KASSERT(task_spawn(group_pid, coordinator, NULL, &ids[6]));
    KASSERT(task_spawn(group_pid, cycle_first, NULL, &ids[7]));
    KASSERT(task_spawn(group_pid, cycle_second, NULL, &ids[8]));
    cycle_a = ids[7]; cycle_b = ids[8];
    struct scheduler_stats before, after;
    scheduler_get_stats(&before);
    scheduler_run(true, 2, 500);
    scheduler_get_stats(&after);
    KASSERT(protected_count == 8 && in_critical == 0 && consumed == 2 && joined_child == 1 && joined_cycle == 1);
    KASSERT(mutex.owner == TASK_NONE && semaphore.count == 0 && after.idle_sleeps > before.idle_sleeps);
    for (unsigned int i = 0; i < 9; ++i) {
        if (i == 5 || i == 8) { continue; }
        int status;
        KASSERT(task_join(ids[i], &status) && status == 0);
    }
    KASSERT(task_process_destroy(group_pid));
    klog(LOG_INFO, "scheduler-test", "sync round=%u mutex_updates=8 tokens=2; sleep/idle, blocking join, cancellation and stale handles passed; join cycles passed", round);
    serial_write("AGENTICOS: SYNC PASS\n");
}
#if CONFIG_SELFTEST == 16
static void spawn_oom_test(void) {
    uint64_t pid, physical, count = 0, head = UINT64_MAX, initial = memory_frames()->free_frames;
    KASSERT(task_process_create("OOM", &pid));
    while (pmm_alloc_frame(memory_frames(), &physical)) {
        uint64_t *node = memory_frame_pointer(physical);
        KASSERT(node != NULL);
        *node = head; head = physical; ++count;
    }
    uint64_t unchanged = TASK_NONE;
    KASSERT(count == initial && !task_spawn(pid, immediate, NULL, &unchanged) && unchanged == TASK_NONE);
    while (head != UINT64_MAX) {
        uint64_t next = *(uint64_t *)memory_frame_pointer(head);
        KASSERT(pmm_free_frame(memory_frames(), head)); head = next;
    }
    KASSERT(memory_frames()->free_frames == initial && task_process_destroy(pid));
    klog(LOG_INFO, "scheduler-test", "real stack OOM rejected spawn; all %llu frames recovered", (unsigned long long)count);
}
#endif
void scheduler_self_test(void) {
    uint64_t initial_frames = memory_frames()->free_frames;
    uint64_t initial_tables = memory_address_space()->table_pages;
    uint64_t initial_mappings = memory_address_space()->mapped_pages;
    capacity_test();
#if CONFIG_SELFTEST == 16
    spawn_oom_test();
#endif
    unsigned int rounds = CONFIG_SELFTEST == 16 ? 4u : 1u;
    for (unsigned int round = 1; round <= rounds; ++round) {
        cooperative_test(round);
        preemptive_test(round);
        synchronization_test(round);
        struct scheduler_stats stats;
        scheduler_get_stats(&stats);
        KASSERT(stats.live_tasks == 0 && task_current() == 0);
        KASSERT(memory_frames()->free_frames == initial_frames && memory_address_space()->table_pages == initial_tables &&
                memory_address_space()->mapped_pages == initial_mappings);
        klog(LOG_INFO, "scheduler-test", "reclaim round=%u free_frames=%llu table_pages=%llu mappings=%llu; no task resources retained",
             round, (unsigned long long)initial_frames, (unsigned long long)initial_tables, (unsigned long long)initial_mappings);
        serial_write("AGENTICOS: RECLAIM PASS\n");
    }
}
