#ifndef AGENTICOS_WINDOW_H
#define AGENTICOS_WINDOW_H
#include <stdbool.h>
#include <stdint.h>
#define WM_MAX 8u
#define APP_TERMINAL 1u
#define APP_FILES 2u
#define APP_CALCULATOR 3u
#define APP_EDITOR 4u
#define APP_MONITOR 5u
#define APP_SETTINGS 6u
#define APP_AGENT 7u
struct window { int32_t x,y,width,height; uint32_t app; bool live; };
struct window_manager { struct window windows[WM_MAX]; unsigned int order[WM_MAX], count; int active, dragging, anchor_x, anchor_y, width, height; };
void wm_init(struct window_manager *manager, int width, int height);
int wm_open(struct window_manager *manager, unsigned int app);
void wm_close(struct window_manager *manager, unsigned int slot);
void wm_focus(struct window_manager *manager, unsigned int slot);
int wm_hit(const struct window_manager *manager, int x, int y);
void wm_drag(struct window_manager *manager, int x, int y, bool pressed);
bool wm_calculate(const char *expression, int64_t *result);
#endif
