#include "memory.h"
#include <stdint.h>

/* Volatile bytes prevent optimization into calls to these same primitives.
 * Callers supply valid spans; memcpy requires non-overlapping source/destination. */
void *memcpy(void *restrict destination, const void *restrict source, size_t bytes) {
    volatile unsigned char *output = destination;
    const volatile unsigned char *input = source;
    for (size_t i = 0; i < bytes; ++i) { output[i] = input[i]; }
    return destination;
}
void *memmove(void *destination, const void *source, size_t bytes) {
    volatile unsigned char *output = destination;
    const volatile unsigned char *input = source;
    uintptr_t out_address = (uintptr_t)destination, in_address = (uintptr_t)source;
    if (out_address <= in_address || out_address - in_address >= bytes) {
        for (size_t i = 0; i < bytes; ++i) { output[i] = input[i]; }
    } else {
        for (size_t i = bytes; i != 0; --i) { output[i - 1] = input[i - 1]; }
    }
    return destination;
}
void *memset(void *destination, int value, size_t bytes) {
    volatile unsigned char *output = destination;
    for (size_t i = 0; i < bytes; ++i) { output[i] = (unsigned char)value; }
    return destination;
}
int memcmp(const void *left, const void *right, size_t bytes) {
    const volatile unsigned char *a = left, *b = right;
    for (size_t i = 0; i < bytes; ++i) {
        if (a[i] != b[i]) { return a[i] < b[i] ? -1 : 1; }
    }
    return 0;
}
