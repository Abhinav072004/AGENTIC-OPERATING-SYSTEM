#include "process.h"
#include <limits.h>
#include <stddef.h>
#include "runtime/memory.h"

static void name_copy(char destination[PROCESS_NAME_BYTES], const char *source) {
    if (source == NULL) { source = "kernel group"; }
    size_t i = 0;
    while (i + 1 < PROCESS_NAME_BYTES && source[i] != '\0') { destination[i] = source[i]; ++i; }
    destination[i] = '\0';
}
bool process_table_init(struct process_table *table, uint64_t address_space) {
    if (table == NULL || address_space % 4096 != 0 || address_space >= (UINT64_C(1) << 52)) { return false; }
    memset(table, 0, sizeof(*table));
    table->address_space = address_space;
    table->next_pid = 2;
    table->ready = true;
    table->entries[0] = (struct process_control){.pid = KERNEL_PID, .address_space = address_space, .live = true};
    name_copy(table->entries[0].name, "kernel");
    return true;
}
struct process_control *process_find(struct process_table *table, uint64_t pid) {
    if (table == NULL || !table->ready) { return NULL; }
    for (size_t i = 0; i < PROCESS_CAPACITY; ++i) {
        if (table->entries[i].live && table->entries[i].pid == pid) { return &table->entries[i]; }
    }
    return NULL;
}
bool process_create(struct process_table *table, const char *name, uint64_t *pid) {
    if (table == NULL || !table->ready || pid == NULL || table->next_pid > INT64_MAX) { return false; }
    for (size_t i = 1; i < PROCESS_CAPACITY; ++i) {
        if (table->entries[i].live) { continue; }
        table->entries[i] = (struct process_control){.pid = table->next_pid++,
            .address_space = table->address_space, .live = true};
        name_copy(table->entries[i].name, name);
        *pid = table->entries[i].pid;
        return true;
    }
    return false;
}
bool process_destroy(struct process_table *table, uint64_t pid) {
    struct process_control *process = process_find(table, pid);
    if (process == NULL || pid == KERNEL_PID || process->threads != 0) { return false; }
    memset(process, 0, sizeof(*process));
    return true;
}
bool process_retain_thread(struct process_table *table, uint64_t pid) {
    struct process_control *process = process_find(table, pid);
    if (process == NULL || process->threads == UINT_MAX) { return false; }
    ++process->threads;
    return true;
}
bool process_release_thread(struct process_table *table, uint64_t pid) {
    struct process_control *process = process_find(table, pid);
    if (process == NULL || process->threads == 0) { return false; }
    --process->threads;
    return true;
}
