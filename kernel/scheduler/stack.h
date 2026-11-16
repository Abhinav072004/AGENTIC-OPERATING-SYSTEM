#ifndef AGENTICOS_TASK_STACK_H
#define AGENTICOS_TASK_STACK_H
#include "core.h"
#define TASK_STACK_BASE UINT64_C(0xffffd20000000000)
#define TASK_STACK_PAGES 4u
#define TASK_STACK_BYTES (TASK_STACK_PAGES * UINT64_C(4096))
#define TASK_STACK_STRIDE ((TASK_STACK_PAGES + 2u) * UINT64_C(4096))
struct task_stack_ops {
    void *context;
    bool (*allocate)(void *context, uint64_t *physical);
    bool (*release)(void *context, uint64_t physical);
    bool (*mapped)(void *context, uint64_t address);
    bool (*map)(void *context, uint64_t address, uint64_t physical);
    bool (*unmap)(void *context, uint64_t address, uint64_t *physical);
};
struct task_stack {
    struct task_stack_ops ops;
    uint64_t frames[TASK_STACK_PAGES], base, bottom, top;
    bool ready;
};
/* Call with IF clear. No allocations on failure, and output stays unchanged.
 * Destroy only after the task has switched off this stack. Backend ownership
 * violations during destruction are fatal to the kernel, not recoverable OOM. */
bool task_stack_create(struct task_stack *stack, unsigned int slot, const struct task_stack_ops *ops);
bool task_stack_destroy(struct task_stack *stack);
#endif
