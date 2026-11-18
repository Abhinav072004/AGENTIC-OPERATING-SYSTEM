#include "text.h"
size_t text_length(const char *text) { size_t count = 0; while (text[count] != '\0') { ++count; } return count; }
bool text_equal(const char *a, const char *b) { size_t i = 0; while (a[i] != '\0' && a[i] == b[i]) { ++i; } return a[i] == b[i]; }
void text_copy(char *destination, const char *source, size_t capacity) { if (capacity == 0) { return; } size_t i = 0; while (i + 1 < capacity && source[i] != '\0') { destination[i] = source[i]; ++i; } destination[i] = '\0'; }
/* In-place compaction: read never trails write. Quotes and escapes are syntax,
 * never expansion or implicit execution. Returns -1 quote, -2 argc, -3 escape. */
int text_split(char *line, char *arguments[TEXT_ARGS_MAX]) {
    size_t read = 0, write = 0; int count = 0;
    while (line[read] != '\0') {
        while (line[read] == ' ' || line[read] == '\t') { ++read; }
        if (line[read] == '\0') { break; }
        if (count == (int)TEXT_ARGS_MAX) { return -2; }
        arguments[count++] = line + write; char quote = '\0';
        while (line[read] != '\0' && (quote != '\0' || (line[read] != ' ' && line[read] != '\t'))) {
            char byte = line[read++];
            if (byte == '\\' && quote != '\'') { if (line[read] == '\0') { return -3; } byte = line[read++]; }
            else if ((byte == '\'' || byte == '"') && (quote == '\0' || quote == byte)) { quote = quote == '\0' ? byte : '\0'; continue; }
            line[write++] = byte;
        }
        if (quote != '\0') { return -1; }
        /* Advance over the delimiter before writing a NUL into its old slot. */
        if (line[read] != '\0') { ++read; }
        line[write++] = '\0';
    }
    return count;
}
bool text_path(const char *cwd, const char *input, char output[128]) {
    char combined[256]; size_t length = 0;
    if (input[0] != '/') {
        size_t base = text_length(cwd); if (base >= 128) { return false; }
        for (size_t i = 0; i < base; ++i) { combined[length++] = cwd[i]; }
        if (length == 0 || combined[length - 1] != '/') { combined[length++] = '/'; }
    }
    for (size_t i = 0; input[i] != '\0'; ++i) { if (length + 1 >= sizeof(combined)) { return false; } combined[length++] = input[i]; }
    combined[length] = '\0'; size_t read = 0, write = 1; output[0] = '/';
    while (read < length) {
        while (combined[read] == '/') { ++read; }
        size_t start = read; while (combined[read] != '\0' && combined[read] != '/') { ++read; }
        size_t count = read - start; if (count == 0 || (count == 1 && combined[start] == '.')) { continue; }
        if (count == 2 && combined[start] == '.' && combined[start + 1] == '.') {
            if (write == 1) { return false; }
            while (write > 1 && output[write - 1] != '/') { --write; } if (write > 1) { --write; } continue;
        }
        if (count > 31 || write + count + (write > 1 ? 1u : 0u) >= 128) { return false; }
        if (write > 1) { output[write++] = '/'; }
        for (size_t i = 0; i < count; ++i) { output[write++] = combined[start + i]; }
    }
    output[write] = '\0'; return true;
}
bool text_unsigned(const char *text, uint64_t *value) {
    if (text[0] == '\0') { return false; } uint64_t number = 0;
    for (size_t i = 0; text[i] != '\0'; ++i) { if (text[i] < '0' || text[i] > '9') { return false; } unsigned int digit = (unsigned int)(text[i] - '0'); if (number > (UINT64_MAX - digit) / 10u) { return false; } number = number * 10u + digit; }
    *value = number; return true;
}
size_t text_number(char *output, int64_t value) {
    char reverse[20]; size_t count = 0, length = 0; uint64_t magnitude = (uint64_t)value;
    if (value < 0) { output[length++] = '-'; magnitude = UINT64_C(0) - magnitude; }
    do { reverse[count++] = (char)('0' + magnitude % 10u); magnitude /= 10u; } while (magnitude != 0);
    while (count != 0) { output[length++] = reverse[--count]; } output[length] = '\0'; return length;
}
