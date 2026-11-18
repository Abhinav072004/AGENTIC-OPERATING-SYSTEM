#include "input.h"
#include <stddef.h>
static struct input_event queue[64];
static size_t head, tail;
static uint64_t dropped;
static bool left_shift, right_shift, extended;
static uint8_t packet[3];
static size_t packet_bytes;
void input_reset(void) { head = tail = 0; dropped = 0; left_shift = right_shift = extended = false; packet_bytes = 0; }
uint64_t input_dropped(void) { return dropped; }
static void enqueue(struct input_event event) { size_t next = (tail + 1u) % 64u; if (next == head) { ++dropped; return; } queue[tail] = event; tail = next; }
bool input_pop(struct input_event *event);
bool input_pop(struct input_event *event) { if (head == tail) { return false; } *event = queue[head]; head = (head + 1u) % 64u; return true; }
void input_decode(uint8_t byte, bool mouse) {
    if (mouse) {
        if (packet_bytes == 0 && (byte & 8u) == 0) { return; }
        packet[packet_bytes++] = byte;
        if (packet_bytes != 3) { return; } packet_bytes = 0;
        if ((packet[0] & 0xc0u) != 0) { return; }
        int32_t dx = packet[1], dy = packet[2];
        if ((packet[0] & 0x10u) != 0) { dx -= 256; } if ((packet[0] & 0x20u) != 0) { dy -= 256; }
        enqueue((struct input_event){.kind = INPUT_MOUSE, .dx = dx, .dy = -dy, .buttons = packet[0] & 7u}); return;
    }
    if (byte == 0xe0) { extended = true; return; }
    bool release = (byte & 0x80u) != 0; uint8_t code = byte & 0x7fu;
    if (code == 0x2a && !extended) { left_shift = !release; return; }
    if (code == 0x36 && !extended) { right_shift = !release; return; }
    if (release) { extended = false; return; }
    uint32_t key = 0;
    if (extended) { if (code == 0x48) { key = 256; } else if (code == 0x50) { key = 257; } else if (code == 0x4b) { key = 258; } else if (code == 0x4d) { key = 259; } }
    else {
        static const char normal[58] = "\0\0331234567890-=\b\tqwertyuiop[]\n\0asdfghjkl;'`\0\\zxcvbnm,./\0*\0 ";
        static const char shifted[58] = "\0\033!@#$%^&*()_+\b\tQWERTYUIOP{}\n\0ASDFGHJKL:\"~\0|ZXCVBNM<>?\0*\0 ";
        if (code < sizeof(normal)) { key = (uint32_t)(unsigned char)((left_shift || right_shift) ? shifted[code] : normal[code]); }
    }
    extended = false; if (key != 0) { enqueue((struct input_event){.kind = INPUT_KEY, .key = key}); }
}
