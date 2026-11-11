#include "panic.h"
#include "log.h"
#include "format.h"
#include "arch/x86_64/diagnostics.h"
#include <stdarg.h>

_Noreturn void panic_at(const char *file, unsigned int line, const char *function,
                       const char *format, ...) {
    arch_disable_interrupts();
    char reason[192];
    va_list args;
    va_start(args, format);
    int length = kvsnprintf(reason, sizeof(reason), format, args);
    va_end(args);
    if (length < 0) {
        (void)ksnprintf(reason, sizeof(reason), "%s", "<invalid panic format>");
    } else if ((unsigned int)length >= sizeof(reason)) {
        reason[sizeof(reason) - 4] = '.';
        reason[sizeof(reason) - 3] = '.';
        reason[sizeof(reason) - 2] = '.';
    }
    klog(LOG_PANIC, "panic", "%s:%u %s: %s", file, line, function, reason);
    arch_panic_stop();
}
