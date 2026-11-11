#ifndef AGENTICOS_FORMAT_H
#define AGENTICOS_FORMAT_H

#include <stdarg.h>
#include <stddef.h>

/* snprintf semantics; -1 for unsupported syntax. No flags, width or precision.
 * Supported: %% %s %c %d %u %x %p, with l/ll/z for unsigned and l/ll for signed.
 * A nonzero capacity requires a valid destination; strings are borrowed. */
int kvsnprintf(char *destination, size_t capacity, const char *format, va_list args);
int ksnprintf(char *destination, size_t capacity, const char *format, ...)
    __attribute__((format(printf, 3, 4)));

#endif
