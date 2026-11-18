#ifndef AGENTICOS_INPUT_H
#define AGENTICOS_INPUT_H
#include <stdbool.h>
#include <stdint.h>
#define INPUT_KEY 1u
#define INPUT_MOUSE 2u
struct input_event { uint32_t kind, key; int32_t dx, dy; uint32_t buttons, modifiers; };
bool input_init(void);
bool input_next(struct input_event *event);
void input_decode(uint8_t byte, bool mouse);
void input_reset(void);
uint64_t input_dropped(void);
#endif
