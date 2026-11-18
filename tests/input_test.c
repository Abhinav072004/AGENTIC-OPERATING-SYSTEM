#include <assert.h>
#include <stdio.h>
#include "drivers/input.h"
bool input_pop(struct input_event *event);
int main(void) {
    struct input_event event; input_reset();
    input_decode(0x2a, false); input_decode(0x36, false); input_decode(0xaa, false); input_decode(0x1e, false);
    assert(input_pop(&event) && event.kind == INPUT_KEY && event.key == 'A');
    input_decode(0xb6, false); input_decode(0x1e, false); assert(input_pop(&event) && event.key == 'a');
    input_decode(0xe0, false); input_decode(0x48, false); assert(input_pop(&event) && event.key == 256);
    input_decode(0xe0, false); input_decode(0xc8, false); assert(!input_pop(&event));
    input_decode(0x1d,false); input_decode(0xe0,false); input_decode(0x1d,false); input_decode(0x9d,false); input_decode(0x1f,false);
    assert(input_pop(&event) && event.key == 's' && event.modifiers == 1);
    input_decode(0xe0,false); input_decode(0x9d,false); input_decode(0x1f,false); assert(input_pop(&event) && event.modifiers == 0);
    input_decode(0, true); input_decode(0x39, true); input_decode(0xfb, true); input_decode(0xfd, true);
    assert(input_pop(&event) && event.kind == INPUT_MOUSE && event.dx == -5 && event.dy == 3 && event.buttons == 1);
    input_decode(0xc8, true); input_decode(1, true); input_decode(1, true); assert(!input_pop(&event));
    for (unsigned int i = 0; i < 100; ++i) { input_decode(0x1e, false); }
    assert(input_dropped() == 37); unsigned int count = 0; while (input_pop(&event)) { ++count; } assert(count == 63);
    puts("PASS: PS/2 shifts, extended key releases, mouse sign/synchronization/overflow and bounded event queue"); return 0;
}
