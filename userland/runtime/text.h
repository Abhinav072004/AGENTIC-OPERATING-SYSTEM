#ifndef AGENTICOS_TEXT_H
#define AGENTICOS_TEXT_H
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#define TEXT_LINE_MAX 256u
#define TEXT_ARGS_MAX 16u
size_t text_length(const char *text);
bool text_equal(const char *a, const char *b);
void text_copy(char *destination, const char *source, size_t capacity);
int text_split(char *line, char *arguments[TEXT_ARGS_MAX]);
bool text_path(const char *cwd, const char *input, char output[128]);
bool text_unsigned(const char *text, uint64_t *value);
size_t text_number(char *output, int64_t value);
#endif
