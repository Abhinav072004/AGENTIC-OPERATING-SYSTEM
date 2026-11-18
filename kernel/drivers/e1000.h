#ifndef AGENTICOS_E1000_H
#define AGENTICOS_E1000_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
bool e1000_init(void);
bool e1000_ready(void);
const uint8_t *e1000_mac(void);
bool e1000_send(const void *frame, size_t length);
size_t e1000_receive(void *frame, size_t capacity);
#endif
