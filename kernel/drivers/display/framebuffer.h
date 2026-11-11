#ifndef ASTRA_FRAMEBUFFER_H
#define ASTRA_FRAMEBUFFER_H

#include <stdbool.h>
#include <limine.h>

/* Borrows bootloader-owned metadata; no allocation or ownership transfer. */
bool framebuffer_welcome(const struct limine_framebuffer *fb);

#endif
