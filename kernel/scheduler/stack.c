#include "stack.h"
#include "runtime/memory.h"

bool task_stack_create(struct task_stack *stack, unsigned int slot, const struct task_stack_ops *ops) {
    if (stack == NULL || stack->ready || slot == 0 || slot >= TASK_CAPACITY || ops == NULL ||
        ops->allocate == NULL || ops->release == NULL || ops->mapped == NULL ||
        ops->map == NULL || ops->unmap == NULL) { return false; }
    struct task_stack result = {.ops = *ops, .base = TASK_STACK_BASE + slot * TASK_STACK_STRIDE};
    result.bottom = result.base + 4096;
    result.top = result.bottom + TASK_STACK_BYTES;
    for (unsigned int i = 0; i < TASK_STACK_PAGES + 2u; ++i) {
        if (ops->mapped(ops->context, result.base + i * UINT64_C(4096))) { return false; }
    }
    unsigned int mapped = 0;
    for (; mapped < TASK_STACK_PAGES; ++mapped) {
        uint64_t physical;
        if (!ops->allocate(ops->context, &physical)) { break; }
        if (!ops->map(ops->context, result.bottom + mapped * UINT64_C(4096), physical)) {
            if (!ops->release(ops->context, physical)) { return false; }
            break;
        }
        result.frames[mapped] = physical;
    }
    if (mapped != TASK_STACK_PAGES) {
        while (mapped != 0) {
            uint64_t physical;
            --mapped;
            if (!ops->unmap(ops->context, result.bottom + mapped * UINT64_C(4096), &physical) ||
                physical != result.frames[mapped] || !ops->release(ops->context, physical)) { return false; }
        }
        return false;
    }
    result.ready = true;
    *stack = result;
    return true;
}
bool task_stack_destroy(struct task_stack *stack) {
    if (stack == NULL || !stack->ready) { return false; }
    for (unsigned int i = 0; i < TASK_STACK_PAGES; ++i) {
        uint64_t physical;
        if (!stack->ops.unmap(stack->ops.context, stack->bottom + i * UINT64_C(4096), &physical) ||
            physical != stack->frames[i] || !stack->ops.release(stack->ops.context, physical)) { return false; }
    }
    memset(stack, 0, sizeof(*stack));
    return true;
}
