#include "log.h"
#include "format.h"
#include <stddef.h>
#include <stdint.h>

static log_sink_fn output_sink;
static void *output_context;
static enum log_level minimum_level;
static uint64_t sequence;

void log_init(log_sink_fn sink, void *context, enum log_level minimum) {
    output_sink = sink;
    output_context = context;
    minimum_level = minimum >= LOG_DEBUG && minimum <= LOG_PANIC ? minimum : LOG_INFO;
    sequence = 0;
}

static void sanitize(char *value) {
    /* Keep one record per line, including when a diagnostic contains controls. */
    for (; *value != '\0'; ++value) {
        if ((unsigned char)*value < 32 || (unsigned char)*value == 127) { *value = '?'; }
    }
}

void klogv(enum log_level level, const char *component, const char *format, va_list args) {
    static const char *const names[] = {"DEBUG", "INFO", "WARNING", "ERROR", "PANIC"};
    if (output_sink == NULL || level < LOG_DEBUG || level > LOG_PANIC || level < minimum_level) {
        return;
    }
    char message[256];
    char label[32];
    char record[352];
    int length = kvsnprintf(message, sizeof(message), format, args);
    if (length < 0) {
        (void)ksnprintf(message, sizeof(message), "%s", "<invalid log format>");
    } else if ((size_t)length >= sizeof(message)) {
        /* Truncation is explicit rather than silently losing diagnostic context. */
        message[sizeof(message) - 4] = '.';
        message[sizeof(message) - 3] = '.';
        message[sizeof(message) - 2] = '.';
    }
    (void)ksnprintf(label, sizeof(label), "%s", component == NULL ? "kernel" : component);
    sanitize(label);
    sanitize(message);
    (void)ksnprintf(record, sizeof(record), "[%llu] [%s] [%s] %s\n",
                   (unsigned long long)++sequence, names[level], label, message);
    output_sink(record, output_context);
}

void klog(enum log_level level, const char *component, const char *format, ...) {
    va_list args;
    va_start(args, format);
    klogv(level, component, format, args);
    va_end(args);
}
