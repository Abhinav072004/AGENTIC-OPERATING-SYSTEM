#ifndef AGENTICOS_GUI_DISPLAY_H
#define AGENTICOS_GUI_DISPLAY_H
#include <limine.h>
#include <stdbool.h>
#include "abi.h"
bool display_init(const struct limine_framebuffer *framebuffer);
void display_set_buffer(void *pixels);
bool display_present(void);
bool gui_buffer_create(void);
void gui_buffer_destroy(void);
bool display_draw(const struct gui_draw *draw);
struct gui_info display_info(void);
void applications_boot(const struct limine_framebuffer *framebuffer);
void desktop_session(const struct limine_framebuffer *framebuffer);
void desktop_boot(const struct limine_framebuffer *framebuffer);
#endif
