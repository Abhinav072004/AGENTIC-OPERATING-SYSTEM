#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "drivers/display/framebuffer.h"

int main(void) {
    enum { WIDTH = 400, HEIGHT = 100, PITCH = WIDTH * 4 + 16, GUARD = 32 };
    size_t size = (size_t)PITCH * HEIGHT;
    uint8_t *allocation = malloc(size + GUARD * 2);
    assert(allocation != NULL);
    memset(allocation, 0xa5, size + GUARD * 2);
    struct limine_framebuffer fb = {
        .address = allocation + GUARD, .width = WIDTH, .height = HEIGHT,
        .pitch = PITCH, .bpp = 32, .memory_model = LIMINE_FRAMEBUFFER_RGB,
        .red_mask_size = 8, .red_mask_shift = 16,
        .green_mask_size = 8, .green_mask_shift = 8,
        .blue_mask_size = 8, .blue_mask_shift = 0,
    };
    assert(framebuffer_welcome(&fb));
    uint32_t background;
    memcpy(&background, fb.address, sizeof(background));
    assert(background == UINT32_C(0x0c1220));
    unsigned int ink = 0;
    for (size_t y = 0; y < HEIGHT; ++y) {
        for (size_t x = 0; x < WIDTH; ++x) {
            uint32_t color;
            memcpy(&color, allocation + GUARD + y * PITCH + x * 4, sizeof(color));
            assert(color == background || color == UINT32_C(0xe1ebfa));
            if (color != background) { ++ink; }
        }
        for (size_t x = WIDTH * 4; x < PITCH; ++x) {
            assert(allocation[GUARD + y * PITCH + x] == 0xa5);
        }
    }
    assert(ink > 1000);
    for (size_t i = 0; i < GUARD; ++i) {
        assert(allocation[i] == 0xa5);
        assert(allocation[GUARD + size + i] == 0xa5);
    }
    puts("PASS: framebuffer renders text, respects pitch, preserves buffer guards");

    /* Exercise another valid RGB layout, rather than assuming BGR byte order. */
    fb.red_mask_shift = 0;
    fb.blue_mask_shift = 16;
    assert(framebuffer_welcome(&fb));
    memcpy(&background, fb.address, sizeof(background));
    assert(background == UINT32_C(0x20120c));

    struct limine_framebuffer invalid = fb;
    assert(!framebuffer_welcome(NULL));
    invalid.address = NULL; assert(!framebuffer_welcome(&invalid));
    invalid = fb; invalid.width = 1; assert(!framebuffer_welcome(&invalid));
    invalid = fb; invalid.height = 1; assert(!framebuffer_welcome(&invalid));
    invalid = fb; invalid.bpp = 24; assert(!framebuffer_welcome(&invalid));
    invalid = fb; invalid.pitch = 1; assert(!framebuffer_welcome(&invalid));
    invalid = fb; invalid.height = UINT64_MAX; assert(!framebuffer_welcome(&invalid));
    invalid = fb; invalid.red_mask_shift = 32; assert(!framebuffer_welcome(&invalid));
    invalid = fb; invalid.red_mask_shift = 8; assert(!framebuffer_welcome(&invalid));
    invalid = fb; invalid.address = allocation + 1; assert(!framebuffer_welcome(&invalid));
    free(allocation);
    puts("PASS: alternate RGB layout and ten invalid framebuffer cases (ASan/UBSan)");
    return 0;
}
