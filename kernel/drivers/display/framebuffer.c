#include "framebuffer.h"
#include <stddef.h>
#include <stdint.h>

/* Original 5x7 glyphs for the first-boot message only, not a terminal font. */
struct glyph {
    char character;
    uint8_t rows[7];
};

static const struct glyph font[] = {
    {'W', {17,17,17,21,21,21,10}},
    {'e', { 0, 0,14,17,31,16,14}},
    {'l', {12, 4, 4, 4, 4, 4,14}},
    {'c', { 0, 0,14,16,16,17,14}},
    {'o', { 0, 0,14,17,17,17,14}},
    {'m', { 0, 0,26,21,21,21,21}},
    {' ', { 0, 0, 0, 0, 0, 0, 0}},
    {'t', { 4, 4,31, 4, 4, 5, 2}},
    {'A', {14,17,17,31,17,17,17}},
    {'s', { 0, 0,15,16,14, 1,30}},
    {'r', { 0, 0,22,25,16,16,16}},
    {'a', { 0, 0,14, 1,15,17,15}},
    {'O', {14,17,17,17,17,17,14}},
    {'S', {15,16,16,14, 1, 1,30}},
    {'g', { 0, 0,15,17,15, 1,14}},
    {'n', { 0, 0,30,17,17,17,17}},
    {'i', { 4, 0,12, 4, 4, 4,14}},
};

static uint32_t rgb(const struct limine_framebuffer *fb,
                    uint8_t red, uint8_t green, uint8_t blue) {
    return ((uint32_t)red << fb->red_mask_shift) |
           ((uint32_t)green << fb->green_mask_shift) |
           ((uint32_t)blue << fb->blue_mask_shift);
}

static void pixel(const struct limine_framebuffer *fb,
                  uint64_t x, uint64_t y, uint32_t color) {
    volatile uint32_t *row = (volatile uint32_t *)
        ((uintptr_t)fb->address + y * fb->pitch);
    row[x] = color;
}

bool framebuffer_welcome(const struct limine_framebuffer *fb) {
    static const char message[] = "Welcome to AgenticOS";
    const uint64_t scale = 3;
    const uint64_t text_width = (sizeof(message) - 1) * 6 * scale;

    if (fb == NULL || fb->address == NULL || fb->bpp != 32 ||
        fb->memory_model != LIMINE_FRAMEBUFFER_RGB ||
        fb->width < text_width + 32 || fb->height < 100 ||
        fb->width > UINT64_MAX / 4 || fb->pitch < fb->width * 4 ||
        fb->pitch % 4 != 0 || fb->height > UINT64_MAX / fb->pitch ||
        (uintptr_t)fb->address % 4 != 0 ||
        (uintptr_t)fb->address > UINTPTR_MAX - fb->height * fb->pitch ||
        fb->red_mask_size != 8 || fb->green_mask_size != 8 ||
        fb->blue_mask_size != 8 ||
        fb->red_mask_shift > 24 || fb->green_mask_shift > 24 ||
        fb->blue_mask_shift > 24) {
        return false;
    }
    uint32_t red_mask = UINT32_C(255) << fb->red_mask_shift;
    uint32_t green_mask = UINT32_C(255) << fb->green_mask_shift;
    uint32_t blue_mask = UINT32_C(255) << fb->blue_mask_shift;
    if ((red_mask & green_mask) != 0 || (red_mask & blue_mask) != 0 ||
        (green_mask & blue_mask) != 0) {
        return false;
    }

    const uint32_t background = rgb(fb, 12, 18, 32);
    const uint32_t foreground = rgb(fb, 225, 235, 250);
    const uint64_t left = (fb->width - text_width) / 2;
    const uint64_t top = (fb->height - 7 * scale) / 2;

    for (uint64_t y = 0; y < fb->height; ++y) {
        for (uint64_t x = 0; x < fb->width; ++x) {
            pixel(fb, x, y, background);
        }
    }
    for (size_t i = 0; i < sizeof(message) - 1; ++i) {
        for (size_t g = 0; g < sizeof(font) / sizeof(font[0]); ++g) {
            if (font[g].character != message[i]) {
                continue;
            }
            for (uint64_t y = 0; y < 7; ++y) {
                for (uint64_t x = 0; x < 5; ++x) {
                    if ((font[g].rows[y] & (1u << (4 - x))) == 0) {
                        continue;
                    }
                    for (uint64_t dy = 0; dy < scale; ++dy) {
                        for (uint64_t dx = 0; dx < scale; ++dx) {
                            pixel(fb, left + i * 6 * scale + x * scale + dx,
                                  top + y * scale + dy, foreground);
                        }
                    }
                }
            }
            break;
        }
    }
    return true;
}
