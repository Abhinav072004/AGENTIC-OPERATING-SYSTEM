#ifndef AGENTICOS_LOG_H
#define AGENTICOS_LOG_H

#include <stdarg.h>

enum log_level { LOG_DEBUG, LOG_INFO, LOG_WARNING, LOG_ERROR, LOG_PANIC };
typedef void (*log_sink_fn)(const char *record, void *context);

/* One bootstrap CPU, interrupts disabled. Sink borrows each stack-backed record
 * only for the call and must not log recursively. Initialization resets sequence.
 * Caller supplies trusted format strings; PANIC severity bypasses filtering. */
void log_init(log_sink_fn sink, void *context, enum log_level minimum);
void klogv(enum log_level level, const char *component, const char *format, va_list args);
void klog(enum log_level level, const char *component, const char *format, ...)
    __attribute__((format(printf, 3, 4)));

#endif
