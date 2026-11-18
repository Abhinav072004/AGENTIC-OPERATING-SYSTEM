#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gui/display.h"
int main(void) {
    size_t pitch = 804u*4u, bytes = pitch*600u; unsigned char *memory = malloc(bytes+32u); assert(memory != NULL); memset(memory,0xa5,bytes+32u);
    struct limine_framebuffer fb = {.address = memory+16,.width = 800,.height = 600,.pitch = pitch,.bpp = 32,.red_mask_shift = 16,.green_mask_shift = 8,.blue_mask_shift = 0};
    assert(display_init(&fb)); struct gui_info info = display_info(); assert(info.width == 800 && info.height == 600);
    struct gui_draw draw = {.operation = GUI_RECT,.x = -10,.y = -20,.width = 30,.height = 40,.color = 0x123456}; assert(display_draw(&draw));
    assert(*(uint32_t *)(memory+16) == 0x123456); assert(*(uint32_t *)(memory+16+20*4) == 0xa5a5a5a5);
    draw.x = INT_MIN; draw.y = INT_MAX; draw.width = INT_MAX; draw.height = INT_MAX; assert(display_draw(&draw));
    draw.operation = GUI_TEXT; draw.x = 790; draw.y = 596; memset(draw.text,'W',sizeof(draw.text)); assert(display_draw(&draw));
    for (size_t i = 0; i < 16; ++i) { assert(memory[i] == 0xa5 && memory[bytes+16+i] == 0xa5); }
    for (size_t y = 0; y < 600; ++y) { for (size_t i = 800u*4u; i < pitch; ++i) { assert(memory[16+y*pitch+i] == 0xa5); } }
    uint32_t *canvas = malloc(800u*600u*4u); assert(canvas != NULL); for (size_t i = 0; i < 800u*600u; ++i) { canvas[i] = 0xabcdef; }
    display_set_buffer(canvas); draw = (struct gui_draw){.operation = GUI_RECT,.width = 1,.height = 1,.color = 0x55aa77}; assert(display_draw(&draw));
    assert(canvas[0] == 0x55aa77 && *(uint32_t *)(memory+16) == 0x123456); assert(display_present()); assert(*(uint32_t *)(memory+16) == 0x55aa77);
    for (size_t y = 0; y < 600; ++y) { for (size_t i = 800u*4u; i < pitch; ++i) { assert(memory[16+y*pitch+i] == 0xa5); } }
    uint64_t masks[95]={0};
    for(unsigned int code=32;code<=126;++code) {
        memset(canvas,0,800u*7u*4u); draw=(struct gui_draw){.operation=GUI_TEXT,.color=0xffffff}; draw.text[0]=(char)code; assert(display_draw(&draw));
        for(size_t y=0;y<7;++y) { for(size_t x=0;x<5;++x) { if(canvas[y*800u+x]!=0) { masks[code-32u]|=UINT64_C(1)<<(y*5u+x); } } }
        assert(code==32 ? masks[0]==0 : masks[code-32u]!=0);
    }
    for(unsigned int code='a';code<='z';++code) { assert(masks[code-32u]!=masks[code-'a'+'A'-32u]); }
    assert(masks['\\'-32u]!=masks['?'-32u] && masks['_' -32u]!=masks['-'-32u]);
    display_set_buffer(NULL); free(canvas);
    draw.operation = 99; assert(!display_draw(&draw)); draw.operation = GUI_RECT; draw.width = -1; assert(!display_draw(&draw)); free(memory);
    puts("PASS: framebuffer clip boundaries, extreme signed coordinates, unterminated text bounds, pitch padding, back-buffer present surrounding canaries and all 95 printable glyphs with distinct case/punctuation"); return 0;
}
