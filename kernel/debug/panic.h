#ifndef AGENTICOS_PANIC_H
#define AGENTICOS_PANIC_H

_Noreturn void panic_at(const char *file, unsigned int line, const char *function,
                       const char *format, ...) __attribute__((format(printf, 4, 5)));

#define PANIC(...) panic_at(__FILE__, __LINE__, __func__, __VA_ARGS__)
/* Never compiled out by NDEBUG. The expression is evaluated exactly once. */
#define KASSERT(expression) do { \
    if (!(expression)) { PANIC("assertion failed: %s", #expression); } \
} while (0)

#endif
