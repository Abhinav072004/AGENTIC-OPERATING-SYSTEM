#ifndef AGENTICOS_USER_H
#define AGENTICOS_USER_H
#include "space.h"
#include "arch/x86_64/interrupts.h"
#define USER_FAULT_STATUS(vector) (-256 - (int)(vector))
#define USER_BAD_CONTEXT (-300)
struct user_process {
    struct user_space space;
    uint64_t pid, tid, calls[6], rejected_calls, timer_entries;
    const char *name;
    bool live, entered;
};
struct user_process *user_find(uint64_t pid);
bool user_process_start(const void *image, size_t length, const char *name, uint64_t mode,
                        bool private_page, uint64_t *pid, uint64_t *tid);
bool user_process_reap(uint64_t pid, int *status);
bool user_switch_root(uint64_t root);
bool user_frame_prepare(struct interrupt_frame *frame);
void user_bad_context(const struct interrupt_frame *frame);
void user_timer_entry(const struct interrupt_frame *frame);
struct interrupt_frame *user_return(struct interrupt_frame *frame);
struct interrupt_frame *user_fault(struct interrupt_frame *frame);
struct interrupt_frame *syscall_dispatch(struct interrupt_frame *frame);
void user_self_test(void);
#endif
