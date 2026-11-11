#include "format.h"
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>

struct output {
    char *data;
    size_t capacity;
    size_t count;
};

static void emit(struct output *out, char character) {
    if (out->capacity != 0 && out->count < out->capacity - 1) {
        out->data[out->count] = character;
    }
    if (out->count < (size_t)INT_MAX) {
        ++out->count;
    }
}

static void text(struct output *out, const char *value) {
    if (value == NULL) { value = "(null)"; }
    while (*value != '\0') { emit(out, *value++); }
}

static void number(struct output *out, uint64_t value, unsigned int base) {
    static const char digits[] = "0123456789abcdef";
    char reversed[20]; /* UINT64_MAX has 20 decimal digits, 16 hex digits. */
    size_t count = 0;
    do {
        reversed[count++] = digits[value % base];
        value /= base;
    } while (value != 0);
    while (count != 0) { emit(out, reversed[--count]); }
}

int kvsnprintf(char *destination, size_t capacity, const char *format, va_list args) {
    if ((capacity != 0 && destination == NULL) || format == NULL) { return -1; }
    struct output out = {destination, capacity, 0};
    bool valid = true;
    while (*format != '\0' && valid) {
        if (*format != '%') { emit(&out, *format++); continue; }
        ++format;
        enum { NORMAL, LONG, LONG_LONG, SIZE } length = NORMAL;
        if (*format == 'l') {
            length = LONG;
            if (*++format == 'l') { length = LONG_LONG; ++format; }
        } else if (*format == 'z') { length = SIZE; ++format; }
        char specifier = *format;
        if (specifier == '\0') { valid = false; break; }
        ++format;
        switch (specifier) {
        case '%':
            if (length != NORMAL) { valid = false; break; }
            emit(&out, '%'); break;
        case 's':
            if (length != NORMAL) { valid = false; break; }
            text(&out, va_arg(args, const char *)); break;
        case 'c':
            if (length != NORMAL) { valid = false; break; }
            emit(&out, (char)va_arg(args, int)); break;
        case 'd': {
            if (length == SIZE) { valid = false; break; }
            int64_t value = length == LONG_LONG ? va_arg(args, long long) :
                            length == LONG ? va_arg(args, long) : va_arg(args, int);
            if (value < 0) { emit(&out, '-'); }
            /* Unsigned subtraction also handles INT64_MIN without signed overflow. */
            uint64_t magnitude = value < 0 ? UINT64_C(0) - (uint64_t)value : (uint64_t)value;
            number(&out, magnitude, 10); break;
        }
        case 'u':
        case 'x': {
            uint64_t value = length == LONG_LONG ? va_arg(args, unsigned long long) :
                             length == LONG ? va_arg(args, unsigned long) :
                             length == SIZE ? va_arg(args, size_t) : va_arg(args, unsigned int);
            number(&out, value, specifier == 'x' ? 16u : 10u); break;
        }
        case 'p':
            if (length != NORMAL) { valid = false; break; }
            text(&out, "0x"); number(&out, (uintptr_t)va_arg(args, void *), 16); break;
        default: valid = false; break;
        }
    }
    if (capacity != 0) {
        destination[out.count < capacity ? out.count : capacity - 1] = '\0';
    }
    return valid && out.count < (size_t)INT_MAX ? (int)out.count : -1;
}

int ksnprintf(char *destination, size_t capacity, const char *format, ...) {
    va_list args;
    va_start(args, format);
    int result = kvsnprintf(destination, capacity, format, args);
    va_end(args);
    return result;
}
