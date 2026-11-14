#ifndef AGENTICOS_RUNTIME_MEMORY_H
#define AGENTICOS_RUNTIME_MEMORY_H
#include <stddef.h>
void *memcpy(void *restrict destination, const void *restrict source, size_t bytes);
void *memmove(void *destination, const void *source, size_t bytes);
void *memset(void *destination, int value, size_t bytes);
int memcmp(const void *left, const void *right, size_t bytes);
#endif
