#include "display.h"
#include <stddef.h>
/* Original 5x7 printable ASCII glyphs. Preserve case and punctuation for
 * review of case-sensitive paths and literal agent/tool data. */
static const uint8_t glyphs[95][7] = {
 {0,0,0,0,0,0,0},{4,4,4,4,4,0,4},{10,10,10,0,0,0,0},{10,31,10,10,31,10,0},
 {4,15,20,14,5,30,4},{24,25,2,4,8,19,3},{12,18,20,8,21,18,13},{4,4,8,0,0,0,0},
 {2,4,8,8,8,4,2},{8,4,2,2,2,4,8},{0,21,14,31,14,21,0},{0,4,4,31,4,4,0},
 {0,0,0,0,4,4,8},{0,0,0,31,0,0,0},{0,0,0,0,0,12,12},{1,2,2,4,8,8,16},
 {14,17,19,21,25,17,14},{4,12,4,4,4,4,14},{14,17,1,2,4,8,31},{30,1,1,14,1,1,30},
 {2,6,10,18,31,2,2},{31,16,16,30,1,1,30},{14,16,16,30,17,17,14},{31,1,2,4,8,8,8},
 {14,17,17,14,17,17,14},{14,17,17,15,1,1,14},{0,12,12,0,12,12,0},{0,12,12,0,4,4,8},
 {2,4,8,16,8,4,2},{0,0,31,0,31,0,0},{8,4,2,1,2,4,8},{14,17,1,2,4,0,4},
 {14,17,23,21,23,16,14},{14,17,17,31,17,17,17},{30,17,17,30,17,17,30},{14,17,16,16,16,17,14},
 {30,17,17,17,17,17,30},{31,16,16,30,16,16,31},{31,16,16,30,16,16,16},{14,17,16,23,17,17,15},
 {17,17,17,31,17,17,17},{14,4,4,4,4,4,14},{7,2,2,2,2,18,12},{17,18,20,24,20,18,17},
 {16,16,16,16,16,16,31},{17,27,21,21,17,17,17},{17,25,21,19,17,17,17},{14,17,17,17,17,17,14},
 {30,17,17,30,16,16,16},{14,17,17,17,21,18,13},{30,17,17,30,20,18,17},{15,16,16,14,1,1,30},
 {31,4,4,4,4,4,4},{17,17,17,17,17,17,14},{17,17,17,17,17,10,4},{17,17,17,21,21,21,10},
 {17,17,10,4,10,17,17},{17,17,10,4,4,4,4},{31,1,2,4,8,16,31},
 {14,8,8,8,8,8,14},{16,8,8,4,2,2,1},{14,2,2,2,2,2,14},{4,10,17,0,0,0,0},
 {0,0,0,0,0,0,31},{8,4,2,0,0,0,0},
 {0,0,14,1,15,17,15},{16,16,22,25,17,17,30},{0,0,14,16,16,17,14},
 {1,1,13,19,17,17,15},{0,0,14,17,31,16,14},{6,9,8,28,8,8,8},
 {0,0,15,17,15,1,14},{16,16,22,25,17,17,17},{4,0,12,4,4,4,14},
 {2,0,6,2,2,18,12},{16,16,18,20,24,20,18},{12,4,4,4,4,4,14},
 {0,0,26,21,21,21,21},{0,0,22,25,17,17,17},{0,0,14,17,17,17,14},
 {0,0,30,17,30,16,16},{0,0,15,17,15,1,1},{0,0,22,25,16,16,16},
 {0,0,15,16,14,1,30},{8,8,28,8,8,9,6},{0,0,17,17,17,19,13},
 {0,0,17,17,17,10,4},{0,0,17,17,21,21,10},{0,0,17,10,4,10,17},
 {0,0,17,17,15,1,14},{0,0,31,2,4,8,31},
 {2,4,4,8,4,4,2},{4,4,4,4,4,4,4},{8,4,4,2,4,4,8},{0,0,9,22,0,0,0}
};
static const struct limine_framebuffer *fb;
static uint32_t *back_buffer;
void display_set_buffer(void *pixels) { back_buffer = pixels; }
bool display_present(void) {
    if (fb == NULL) { return false; }
    if (back_buffer == NULL) { return true; }
    for (uint64_t y = 0; y < fb->height; ++y) { volatile uint32_t *row = (volatile uint32_t *)((uintptr_t)fb->address+y*fb->pitch); for (uint64_t x = 0; x < fb->width; ++x) { row[x] = back_buffer[y*fb->width+x]; } }
    return true;
}
static uint32_t color(uint32_t rgb) { return ((rgb >> 16) & 255u) << fb->red_mask_shift | ((rgb >> 8) & 255u) << fb->green_mask_shift | (rgb & 255u) << fb->blue_mask_shift; }
static void pixel(int64_t x, int64_t y, uint32_t value) { if (x < 0 || y < 0 || (uint64_t)x >= fb->width || (uint64_t)y >= fb->height) { return; } volatile uint32_t *row = (volatile uint32_t *)((uintptr_t)fb->address + (uint64_t)y * fb->pitch); if (back_buffer != NULL) { back_buffer[(uint64_t)y*fb->width+(uint64_t)x] = value; } else { row[x] = value; } }
/* The boot welcome validator has already checked mask/pitch/address geometry. */
bool display_init(const struct limine_framebuffer *framebuffer) { if (framebuffer == NULL || framebuffer->width > 1920 || framebuffer->height > 1080 || framebuffer->width < 800 || framebuffer->height < 600 || framebuffer->bpp != 32) { return false; } fb = framebuffer; return true; }
struct gui_info display_info(void) { return fb == NULL ? (struct gui_info){0} : (struct gui_info){(uint32_t)fb->width, (uint32_t)fb->height}; }
bool display_draw(const struct gui_draw *draw) {
    if (fb == NULL || draw == NULL) { return false; } uint32_t value = color(draw->color);
    if (draw->operation == GUI_RECT) {
        if (draw->width < 0 || draw->height < 0) { return false; }
        int64_t left = draw->x, top = draw->y, right = left + draw->width, bottom = top + draw->height;
        if (left < 0) { left = 0; } if (top < 0) { top = 0; } if (right > (int64_t)fb->width) { right = (int64_t)fb->width; } if (bottom > (int64_t)fb->height) { bottom = (int64_t)fb->height; }
        for (int64_t y = top; y < bottom; ++y) { for (int64_t x = left; x < right; ++x) { pixel(x, y, value); } } return true;
    }
    if (draw->operation != GUI_TEXT) { return false; }
    for (size_t i = 0; i < sizeof(draw->text) && draw->text[i] != '\0'; ++i) {
        unsigned char character = (unsigned char)draw->text[i];
        if (character < 32 || character > 126) { character = '?'; }
        for (int64_t y = 0; y < 7; ++y) { for (int64_t x = 0; x < 5; ++x) { if ((glyphs[character - 32u][y] & (1u << (4 - x))) != 0) { pixel((int64_t)draw->x + (int64_t)i * 6 + x, (int64_t)draw->y + y, value); } } }
    } return true;
}
