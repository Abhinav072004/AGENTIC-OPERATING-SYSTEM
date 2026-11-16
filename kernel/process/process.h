#ifndef AGENTICOS_PROCESS_H
#define AGENTICOS_PROCESS_H
#include <stdbool.h>
#include <stdint.h>
#define PROCESS_CAPACITY 4u
#define PROCESS_NAME_BYTES 24u
#define KERNEL_PID UINT64_C(1)
/* Week 5 groups kernel threads; all PCBs borrow the same kernel CR3. */
struct process_control {
    uint64_t pid, address_space;
    unsigned int threads;
    bool live;
    char name[PROCESS_NAME_BYTES];
};
struct process_table {
    struct process_control entries[PROCESS_CAPACITY];
    uint64_t next_pid, address_space;
    bool ready;
};
bool process_table_init(struct process_table *table, uint64_t address_space);
struct process_control *process_find(struct process_table *table, uint64_t pid);
bool process_create(struct process_table *table, const char *name, uint64_t *pid);
bool process_destroy(struct process_table *table, uint64_t pid);
bool process_retain_thread(struct process_table *table, uint64_t pid);
bool process_release_thread(struct process_table *table, uint64_t pid);
#endif
