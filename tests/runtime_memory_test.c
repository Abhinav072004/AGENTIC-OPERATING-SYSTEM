/* Avoid overriding host libc/sanitizer interceptors: link renamed primitives. */
#define memcpy agenticos_memcpy
#define memmove agenticos_memmove
#define memset agenticos_memset
#define memcmp agenticos_memcmp
#include "runtime/memory.h"
#undef memcpy
#undef memmove
#undef memset
#undef memcmp
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int sign(int value) { return value < 0 ? -1 : value > 0 ? 1 : 0; }
int main(void) {
    unsigned char source[96], expected[96], actual[96];
    for (size_t i = 0; i < sizeof(source); ++i) { source[i] = (unsigned char)(i * 17 + 3); }
    for (size_t offset = 0; offset < 32; ++offset) {
        for (size_t length = 0; length <= 64; ++length) {
            memset(expected, 0xa5, sizeof(expected)); memset(actual, 0xa5, sizeof(actual));
            memcpy(expected + offset, source, length);
            assert(agenticos_memcpy(actual + offset, source, length) == actual + offset);
            assert(memcmp(expected, actual, sizeof(actual)) == 0);
            memset(expected + offset, 0x17e, length);
            assert(agenticos_memset(actual + offset, 0x17e, length) == actual + offset);
            assert(memcmp(expected, actual, sizeof(actual)) == 0);
            memcpy(expected, source, sizeof(source)); memcpy(actual, source, sizeof(source));
            memmove(expected + offset, expected, length);
            assert(agenticos_memmove(actual + offset, actual, length) == actual + offset);
            assert(memcmp(expected, actual, sizeof(actual)) == 0);
            memcpy(expected, source, sizeof(source)); memcpy(actual, source, sizeof(source));
            memmove(expected, expected + offset, length);
            assert(agenticos_memmove(actual, actual + offset, length) == actual);
            assert(memcmp(expected, actual, sizeof(actual)) == 0);
            assert(agenticos_memcmp(expected, actual, sizeof(actual)) == 0);
        }
    }
    memcpy(expected, source, sizeof(source)); expected[0] ^= 0xff;
    assert(sign(agenticos_memcmp(source, expected, sizeof(source))) == sign(memcmp(source, expected, sizeof(source))));
    assert(sign(agenticos_memcmp(expected, source, sizeof(source))) == sign(memcmp(expected, source, sizeof(source))));
    puts("PASS: compiler memory primitives match libc across bounds and overlapping moves");
    return 0;
}
