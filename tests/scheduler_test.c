#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "scheduler/core.h"
#include "scheduler/stack.h"

static void process_test(void) {
    struct process_table table;
    assert(!process_table_init(NULL, 0) && !process_table_init(&table, 1));
    assert(!process_table_init(&table, UINT64_C(1) << 52));
    assert(process_table_init(&table, 0x1000));
    assert(process_find(&table, 1)->address_space == 0x1000 && !process_destroy(&table, 1));
    uint64_t pids[3], output = 99;
    for (unsigned int i = 0; i < 3; ++i) { assert(process_create(&table, "abcdefghijklmnopqrstuvwx-more", &pids[i])); }
    assert(!process_create(&table, NULL, &output) && output == 99);
    assert(process_find(&table, pids[0])->name[PROCESS_NAME_BYTES - 1] == '\0');
    assert(process_retain_thread(&table, pids[0]) && !process_destroy(&table, pids[0]));
    assert(process_release_thread(&table, pids[0]) && !process_release_thread(&table, pids[0]));
    assert(process_destroy(&table, pids[0]) && process_find(&table, pids[0]) == NULL);
    assert(process_create(&table, NULL, &output) && output > pids[2] && output != pids[0]);
    assert(process_destroy(&table, output));
    table.next_pid = UINT64_C(1) << 63;
    assert(!process_create(&table, "overflow", &output));
    table.next_pid = UINT64_MAX;
    assert(!process_create(&table, NULL, &output));
    puts("PASS: PCB capacity, shared root, names, thread ownership, stale PIDs and ID exhaustion");
}
static void round_robin_test(void) {
    struct scheduler_core core;
    assert(sched_core_init(&core, 0x2000));
    uint64_t ids[15], output = 99;
    for (unsigned int i = 1; i < TASK_CAPACITY; ++i) {
        assert(sched_core_slot(&core) == i);
        assert(sched_core_add(&core, i, 1, 0x1000 + i, &ids[i - 1]));
    }
    assert(core.live_tasks == 15 && process_find(&core.processes, 1)->threads == 16);
    assert(sched_core_slot(&core) == TASK_CAPACITY);
    assert(!sched_core_add(&core, 1, 1, 0x1234, &output) && output == 99);
    assert(!sched_core_add(&core, 0, 1, 0x1234, &output));
    assert(!sched_core_block(&core, WAIT_SLEEP, NULL, 0));
    for (unsigned int i = 0; i < 45; ++i) {
        assert(sched_core_schedule(&core, 0x8000 + i) != 0);
        assert(core.current == i % 15 + 1);
        assert(core.tasks[core.current].state == TASK_RUNNING);
        unsigned int running = 0;
        for (unsigned int j = 0; j < TASK_CAPACITY; ++j) { running += core.tasks[j].state == TASK_RUNNING ? 1u : 0u; }
        assert(running == 1);
    }
    unsigned int old = core.current;
    for (unsigned int i = 0; i < 5; ++i) { assert(sched_core_tick(&core, 0x9000) && core.current == old); }
    core.preemptive = true;
    core.quantum = 2;
    core.slice = 0;
    assert(sched_core_tick(&core, 0xa000) && core.current == old);
    assert(sched_core_tick(&core, 0xa000) && core.current == 1 && core.preemptions == 1);
    puts("PASS: bounded round robin, one RUNNING task, cooperative ticks and two-tick preemption");
}
static void lifecycle_test(void) {
    struct scheduler_core core;
    uint64_t first, second, pid;
    assert(sched_core_init(&core, 0x3000) && process_create(&core.processes, "lifecycle", &pid));
    assert(sched_core_add(&core, 1, pid, 0x1000, &first));
    assert(sched_core_add(&core, 2, pid, 0x2000, &second));
    assert(sched_core_schedule(&core, 0x4000) == 0x1000 && core.current == 1);
    assert(!sched_core_sleep(&core, 0));
    assert(sched_core_sleep(&core, 3));
    assert(sched_core_schedule(&core, 0x1100) == 0x2000 && core.current == 2);
    assert(sched_core_tick(&core, 0x2100) && core.tasks[1].state == TASK_BLOCKED);
    assert(sched_core_tick(&core, 0x2100) && core.tasks[1].state == TASK_BLOCKED);
    assert(sched_core_tick(&core, 0x2100) && core.tasks[1].state == TASK_READY);
    assert(sched_core_schedule(&core, 0x2200) == 0x1100 && core.current == 1);
    core.tasks[2].joiner = first;
    assert(sched_core_block(&core, WAIT_JOIN, NULL, second));
    assert(sched_core_schedule(&core, 0x1200) == 0x2200 && core.current == 2);
    assert(sched_core_finish(&core, second, 42));
    assert(core.tasks[1].state == TASK_READY && !sched_core_reap(&core, second));
    assert(sched_core_finish(&core, first, -125)); /* Cancel after wake, before consuming join. */
    assert(core.tasks[2].joiner == TASK_NONE && core.live_tasks == 0);
    assert(sched_core_schedule(&core, 0x2300) == 0x4000 && core.current == 0);
    assert(sched_core_reap(&core, first) && sched_core_reap(&core, second));
    assert(!sched_core_reap(&core, first) && !sched_core_finish(&core, second, 0));
    assert(process_find(&core.processes, pid)->threads == 0 && process_destroy(&core.processes, pid));
    uint64_t replacement;
    assert(sched_core_add(&core, 1, 1, 0x5555, &replacement) && replacement > second);
    assert(sched_core_find(&core, first) == NULL && sched_core_find(&core, replacement));
    assert(sched_core_schedule(&core, 0x4444) == 0x5555);
    core.tasks[1].held_mutexes = 1;
    assert(!sched_core_finish(&core, replacement, 0));
    core.tasks[1].held_mutexes = 0;
    core.ticks = UINT64_MAX;
    assert(!sched_core_sleep(&core, 1) && sched_core_tick(&core, 0x5555) == 0);
    core.next_wait = UINT64_MAX;
    assert(!sched_core_block(&core, WAIT_MUTEX, &core, 0));
    core.next_tid = TASK_NONE;
    assert(!sched_core_add(&core, 2, 1, 0x6666, &replacement));
    puts("PASS: timed sleep, join wake/cancellation, off-stack reap, stale TIDs, lock-owner and overflow rejection");
}
static void wake_test(void) {
    struct scheduler_core core;
    uint64_t ids[3];
    int mutex, other;
    assert(sched_core_init(&core, 0x4000));
    for (unsigned int i = 1; i <= 3; ++i) { assert(sched_core_add(&core, i, 1, 0x1000 * i, &ids[i - 1])); }
    for (unsigned int i = 0; i < 3; ++i) {
        assert(sched_core_schedule(&core, 0x5000 + i));
        assert(sched_core_block(&core, i == 2 ? WAIT_SEMAPHORE : WAIT_MUTEX, &mutex, 0));
    }
    assert(sched_core_schedule(&core, 0x6000) && core.current == 0);
    assert(sched_core_wake_one(&core, WAIT_MUTEX, &other, 0) == TASK_NONE);
    assert(sched_core_wake_one(&core, WAIT_MUTEX, &mutex, 0) == ids[0]);
    assert(sched_core_wake_one(&core, WAIT_MUTEX, &mutex, 0) == ids[1]);
    assert(sched_core_wake_one(&core, WAIT_MUTEX, &mutex, 0) == TASK_NONE);
    assert(sched_core_wake_one(&core, WAIT_SEMAPHORE, &mutex, 0) == ids[2]);
    puts("PASS: oldest-waiter selection, wait-object isolation and idle fallback when all workers block");
}
struct fake_stack {
    bool allocated[8], mappings[4], collision;
    uint64_t physical[4];
    unsigned int alloc_calls, map_calls, fail_alloc, fail_map;
};
static bool allocate(void *context, uint64_t *physical) {
    struct fake_stack *fake = context;
    if (fake->alloc_calls++ == fake->fail_alloc) { return false; }
    for (unsigned int i = 0; i < 8; ++i) {
        if (!fake->allocated[i]) { fake->allocated[i] = true; *physical = i * UINT64_C(4096); return true; }
    }
    return false;
}
static bool release(void *context, uint64_t physical) {
    struct fake_stack *fake = context;
    assert(physical % 4096 == 0 && physical / 4096 < 8 && fake->allocated[physical / 4096]);
    fake->allocated[physical / 4096] = false;
    return true;
}
static unsigned int page(uint64_t address) {
    uint64_t bottom = TASK_STACK_BASE + TASK_STACK_STRIDE + 4096;
    assert(address >= bottom && address < bottom + TASK_STACK_BYTES && address % 4096 == 0);
    return (unsigned int)((address - bottom) / 4096);
}
static bool mapped(void *context, uint64_t address) {
    struct fake_stack *fake = context;
    uint64_t base = TASK_STACK_BASE + TASK_STACK_STRIDE;
    if (fake->collision) { return address == base; }
    if (address == base || address == base + TASK_STACK_STRIDE - 4096) { return false; }
    return fake->mappings[page(address)];
}
static bool map(void *context, uint64_t address, uint64_t physical) {
    struct fake_stack *fake = context;
    unsigned int index = page(address);
    assert(!fake->mappings[index]);
    if (fake->map_calls++ == fake->fail_map) { return false; }
    fake->mappings[index] = true;
    fake->physical[index] = physical;
    return true;
}
static bool unmap(void *context, uint64_t address, uint64_t *physical) {
    struct fake_stack *fake = context;
    unsigned int index = page(address);
    assert(fake->mappings[index]);
    fake->mappings[index] = false;
    *physical = fake->physical[index];
    return true;
}
static void stack_test(void) {
    for (unsigned int failure = 0; failure < 8; ++failure) {
        struct fake_stack fake = {.fail_alloc = failure < 4 ? failure : 99, .fail_map = failure >= 4 ? failure - 4 : 99};
        const struct task_stack_ops ops = {&fake, allocate, release, mapped, map, unmap};
        struct task_stack stack = {0}, before = stack;
        assert(!task_stack_create(&stack, 1, &ops) && memcmp(&stack, &before, sizeof(stack)) == 0);
        for (unsigned int i = 0; i < 8; ++i) { assert(!fake.allocated[i]); }
        for (unsigned int i = 0; i < 4; ++i) { assert(!fake.mappings[i]); }
    }
    struct fake_stack fake = {.fail_alloc = 99, .fail_map = 99, .collision = true};
    const struct task_stack_ops ops = {&fake, allocate, release, mapped, map, unmap};
    struct task_stack stack = {0};
    assert(!task_stack_create(&stack, 1, &ops) && fake.alloc_calls == 0);
    fake.collision = false;
    assert(!task_stack_create(&stack, 0, &ops) && !task_stack_create(&stack, TASK_CAPACITY, &ops));
    assert(task_stack_create(&stack, 1, &ops) && stack.ready);
    assert(stack.frames[0] == 0 && stack.top - stack.bottom == TASK_STACK_BYTES);
    assert(!mapped(&fake, stack.base) && !mapped(&fake, stack.top));
    assert(!task_stack_create(&stack, 1, &ops));
    assert(task_stack_destroy(&stack) && !stack.ready && !task_stack_destroy(&stack));
    for (unsigned int i = 0; i < 8; ++i) { assert(!fake.allocated[i]); }
    puts("PASS: guarded stack ownership, physical zero, all eight allocation/map rollback points and destruction");
}
int main(void) { process_test(); round_robin_test(); lifecycle_test(); wake_test(); stack_test(); return 0; }
