#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "text.h"
int main(void) {
    char line[TEXT_LINE_MAX] = "write note 'two words'\" plus\" \\\"literal\\\" ''", *args[TEXT_ARGS_MAX];
    assert(text_split(line, args) == 5);
    assert(strcmp(args[2], "two words plus") == 0 && strcmp(args[3], "\"literal\"") == 0 && args[4][0] == 0);
    strcpy(line, "echo '"); assert(text_split(line, args) == -1);
    strcpy(line, "echo \\"); assert(text_split(line, args) == -3);
    strcpy(line, "a a a a a a a a a a a a a a a a a"); assert(text_split(line, args) == -2);
    char path[128]; assert(text_path("/home/a", "../b//./note", path) && strcmp(path, "/home/b/note") == 0);
    assert(!text_path("/", "../escape", path)); assert(text_path("/home/a", "../../etc", path) && strcmp(path, "/etc") == 0);
    uint64_t number; assert(text_unsigned("18446744073709551615", &number) && number == UINT64_MAX);
    assert(!text_unsigned("18446744073709551616", &number) && !text_unsigned("-1", &number) && !text_unsigned("", &number));
    char numeric[22]; assert(text_number(numeric, INT64_MIN) == 20 && strcmp(numeric, "-9223372036854775808") == 0);
    uint32_t random = 42; const char alphabet[] = "abc \t'\"\\";
    for (unsigned int trial = 0; trial < 10000; ++trial) {
        size_t length = trial % 256u;
        for (size_t i = 0; i < length; ++i) { random = random * 1664525u + 1013904223u; line[i] = alphabet[random % (sizeof(alphabet) - 1u)]; }
        line[length] = 0; int count = text_split(line, args);
        if (count >= 0) { assert(count <= (int)TEXT_ARGS_MAX); for (int i = 0; i < count; ++i) { assert(args[i] >= line && args[i] <= line + length); assert(memchr(args[i], 0, (size_t)(line + sizeof(line) - args[i])) != NULL); } }
    }
    puts("PASS: shell quotes/escapes, argument limits, canonical paths, integer overflow and 10,000 bounded parser inputs"); return 0;
}
