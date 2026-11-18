#include "user.h"
#include "scheduler/task.h"
#include "memory/init.h"
#include "arch/x86_64/serial.h"
#include "debug/log.h"
#include "debug/panic.h"
#include "syscall/abi.h"

extern unsigned char user_hello_start[], user_hello_end[], user_probe_start[], user_probe_end[];
static size_t probe_size(void) { return (size_t)((uintptr_t)user_probe_end - (uintptr_t)user_probe_start); }
static void reap(uint64_t pid, int expected) {
    int status;
    KASSERT(user_process_reap(pid, &status) && status == expected);
}
static void normal_pair(unsigned int round) {
    uint64_t pid[2], tid[2];
    for (unsigned int i = 0; i < 2; ++i) {
        KASSERT(user_process_start(user_probe_start, probe_size(), "normal", 0, i == 0, &pid[i], &tid[i]));
    }
    struct user_process *first = user_find(pid[0]), *second = user_find(pid[1]);
    KASSERT(first != NULL && second != NULL && first->space.vm.root != second->space.vm.root);
    uint64_t a, b;
    KASSERT(first->space.writable_address != 0 && first->space.writable_address == second->space.writable_address);
    KASSERT(vmm_query(&first->space.vm, first->space.writable_address, &a, NULL));
    KASSERT(vmm_query(&second->space.vm, second->space.writable_address, &b, NULL) && a != b);
    KASSERT(vmm_query(&first->space.vm, USER_PRIVATE_ADDRESS, NULL, NULL));
    KASSERT(!vmm_query(&second->space.vm, USER_PRIVATE_ADDRESS, NULL, NULL));
    scheduler_run(true, 2, 500);
    for (unsigned int i = 0; i < 2; ++i) {
        struct user_process *process = user_find(pid[i]);
        struct task_control task;
        KASSERT(task_get_info(tid[i], &task) && task.state == TASK_ZOMBIE && task.runtime_ticks >= 6);
        KASSERT(process->entered && process->timer_entries >= 6 && process->rejected_calls >= 8);
        for (unsigned int number = 1; number <= 5; ++number) { KASSERT(process->calls[number] > 0); }
    }
    klog(LOG_INFO, "user-test", "round=%u roots=%p,%p data_frames=%p,%p; CPL3 preemption, private data and five checked syscalls passed",
         round, (void *)(uintptr_t)first->space.vm.root, (void *)(uintptr_t)second->space.vm.root,
         (void *)(uintptr_t)a, (void *)(uintptr_t)b);
    reap(pid[0], 0); reap(pid[1], 0);
    serial_write("AGENTICOS: USER SYSCALL PASS\n");
}
static void isolation_test(uint64_t mode, const char *name, int expected_status) {
    uint64_t guardian, guardian_tid, offender, offender_tid;
    KASSERT(user_process_start(user_probe_start, probe_size(), "guardian", 0, true, &guardian, &guardian_tid));
    KASSERT(user_process_start(user_probe_start, probe_size(), name, mode, false, &offender, &offender_tid));
    scheduler_run(true, 2, 500);
    struct user_process *process = user_find(guardian);
    KASSERT(process != NULL && process->timer_entries >= 6);
    reap(offender, expected_status); reap(guardian, 0);
    klog(LOG_INFO, "user-test", "case=%s status=%d; offender reaped, guardian completed, kernel survived", name, expected_status);
}
#if CONFIG_SELFTEST == 19
static void user_oom_test(void) {
    uint64_t head = UINT64_MAX, physical, initial = memory_frames()->free_frames;
    while (pmm_alloc_frame(memory_frames(), &physical)) {
        uint64_t *node = memory_frame_pointer(physical);
        KASSERT(node != NULL);
        *node = head; head = physical;
    }
    for (unsigned int i = 0; i < 12; ++i) {
        KASSERT(head != UINT64_MAX);
        uint64_t next = *(uint64_t *)memory_frame_pointer(head);
        KASSERT(pmm_free_frame(memory_frames(), head)); head = next;
    }
    uint64_t pid = UINT64_MAX, tid = UINT64_MAX;
    KASSERT(!user_process_start(user_probe_start, probe_size(), "OOM", 0, false, &pid, &tid));
    KASSERT(pid == UINT64_MAX && tid == UINT64_MAX && memory_frames()->free_frames == 12);
    while (head != UINT64_MAX) {
        uint64_t next = *(uint64_t *)memory_frame_pointer(head);
        KASSERT(pmm_free_frame(memory_frames(), head)); head = next;
    }
    KASSERT(memory_frames()->free_frames == initial);
    klog(LOG_INFO, "user-test", "partial loader OOM rolled back; all %llu frames restored", (unsigned long long)initial);
}
#endif
void user_self_test(void) {
    arch_user_cpu_policy();
    uint64_t frames = memory_frames()->free_frames, tables = memory_address_space()->table_pages;
    uint64_t mappings = memory_address_space()->mapped_pages, pid, tid;
#if CONFIG_SELFTEST == 19
    user_oom_test();
#endif
    static const unsigned char invalid[64] = {0};
    uint64_t unchanged_pid = UINT64_MAX, unchanged_tid = UINT64_MAX;
    KASSERT(!user_process_start(invalid, sizeof(invalid), "invalid", 0, false, &unchanged_pid, &unchanged_tid));
    KASSERT(unchanged_pid == UINT64_MAX && unchanged_tid == UINT64_MAX && memory_frames()->free_frames == frames);
    KASSERT(user_process_start(user_hello_start, (size_t)((uintptr_t)user_hello_end - (uintptr_t)user_hello_start), "hello", 0, false, &pid, &tid));
    scheduler_run(true, 2, 500);
    KASSERT(user_find(pid)->entered && user_find(pid)->calls[SYS_WRITE] > 0);
    reap(pid, 0);
    unsigned int rounds = CONFIG_SELFTEST == 19 ? 4u : 1u;
    for (unsigned int round = 1; round <= rounds; ++round) { normal_pair(round); }
    static const char *const names[] = {"kernel", "peer", "readonly", "nx", "cli", "gate", "simd", "context"};
    static const int statuses[] = {USER_FAULT_STATUS(14), USER_FAULT_STATUS(14), USER_FAULT_STATUS(14),
        USER_FAULT_STATUS(14), USER_FAULT_STATUS(13), USER_FAULT_STATUS(13), USER_FAULT_STATUS(6), USER_BAD_CONTEXT};
    for (size_t i = 0; i < 8; ++i) { isolation_test(i + 1, names[i], statuses[i]); }
    serial_write("AGENTICOS: USER ISOLATION PASS\n");
#if CONFIG_SELFTEST == 20
    serial_write("AGENTICOS: USER IO WAIT\n");
    KASSERT(user_process_start(user_probe_start, probe_size(), "read", 9, false, &pid, &tid));
    scheduler_run(true, 2, 500);
    KASSERT(user_find(pid)->calls[SYS_READ] > 0 && user_find(pid)->calls[SYS_WRITE] > 0);
    reap(pid, 0);
    serial_write("AGENTICOS: USER READ PASS\n");
#endif
    KASSERT(memory_address_space()->active && memory_frames()->free_frames == frames &&
            memory_address_space()->table_pages == tables && memory_address_space()->mapped_pages == mappings);
    klog(LOG_INFO, "user-test", "reclaimed all processes: frames=%llu tables=%llu mappings=%llu; kernel CR3 restored",
         (unsigned long long)frames, (unsigned long long)tables, (unsigned long long)mappings);
    serial_write("AGENTICOS: USER RECLAIM PASS\n");
}
