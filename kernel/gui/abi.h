#ifndef AGENTICOS_GUI_ABI_H
#define AGENTICOS_GUI_ABI_H
#include <stdint.h>
#define GUI_RECT 1u
#define GUI_TEXT 2u
struct gui_draw { uint32_t operation; int32_t x, y, width, height; uint32_t color; char text[64]; };
struct gui_info { uint32_t width, height; };
#endif
