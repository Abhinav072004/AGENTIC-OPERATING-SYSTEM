#include "window.h"
#include "text.h"
#include <limits.h>
void wm_init(struct window_manager *manager, int width, int height) { *manager = (struct window_manager){.active = -1, .dragging = -1, .width = width, .height = height}; }
void wm_focus(struct window_manager *manager, unsigned int slot) { if (slot >= WM_MAX || !manager->windows[slot].live) { return; } for (unsigned int i = 0; i < manager->count; ++i) { if (manager->order[i] == slot) { for (unsigned int j = i; j + 1 < manager->count; ++j) { manager->order[j] = manager->order[j + 1]; } manager->order[manager->count - 1] = slot; break; } } manager->active = (int)slot; }
int wm_open(struct window_manager *manager, unsigned int app) { if (app < APP_TERMINAL || app > APP_SETTINGS) { return -1; } for (unsigned int i = 0; i < WM_MAX; ++i) { if (manager->windows[i].live) { continue; } int width = app == APP_CALCULATOR ? 280 : 420, height = 290; int x = app == APP_TERMINAL ? 40 : app == APP_FILES ? 320 : 490, y = app == APP_CALCULATOR ? 240 : 90; if (app == APP_EDITOR) { width = 480; height = 340; x = 180; y = 140; } else if (app == APP_MONITOR) { width = 400; height = 290; x = 220; y = 120; } else if (app == APP_SETTINGS) { width = 380; height = 260; x = 300; y = 160; } if (x + width > manager->width) { x = manager->width - width; } if (y + height > manager->height - 38) { y = manager->height - 38 - height; } manager->windows[i] = (struct window){x,y,width,height,app,true}; manager->order[manager->count++] = i; manager->active = (int)i; return (int)i; } return -1; }
void wm_close(struct window_manager *manager, unsigned int slot) { if (slot >= WM_MAX || !manager->windows[slot].live) { return; } manager->windows[slot].live = false; for (unsigned int i = 0; i < manager->count; ++i) { if (manager->order[i] == slot) { for (unsigned int j = i; j + 1 < manager->count; ++j) { manager->order[j] = manager->order[j + 1]; } --manager->count; break; } } manager->active = manager->count == 0 ? -1 : (int)manager->order[manager->count - 1]; if (manager->dragging == (int)slot) { manager->dragging = -1; } }
int wm_hit(const struct window_manager *manager, int x, int y) { for (unsigned int i = manager->count; i != 0; --i) { unsigned int slot = manager->order[i - 1]; const struct window *window = &manager->windows[slot]; if (x >= window->x && y >= window->y && x < window->x + window->width && y < window->y + window->height) { return (int)slot; } } return -1; }
void wm_drag(struct window_manager *manager, int x, int y, bool pressed) { if (!pressed) { manager->dragging = -1; return; } if (manager->dragging < 0 || manager->dragging >= (int)WM_MAX || !manager->windows[manager->dragging].live) { return; } struct window *window = &manager->windows[manager->dragging]; int64_t left = (int64_t)x - manager->anchor_x, top = (int64_t)y - manager->anchor_y; if (left < 0) { left = 0; } if (top < 44) { top = 44; } if (left > manager->width - window->width) { left = manager->width - window->width; } if (top > manager->height - 38 - window->height) { top = manager->height - 38 - window->height; } window->x = (int)left; window->y = (int)top; }
struct expression_parser { const char *next; unsigned int depth; bool valid; };
static void skip_space(struct expression_parser *parser) { while (*parser->next == ' ' || *parser->next == '\t') { ++parser->next; } }
static int64_t parse_expression(struct expression_parser *parser);
static int64_t literal(struct expression_parser *parser, bool negative) {
    uint64_t magnitude = 0, limit = negative ? UINT64_C(1)<<63 : INT64_MAX; bool any = false;
    while (*parser->next >= '0' && *parser->next <= '9') { unsigned int digit = (unsigned int)(*parser->next++-'0'); any = true; if (magnitude > (limit-digit)/10u) { parser->valid = false; return 0; } magnitude = magnitude*10u+digit; }
    if (!any) { parser->valid = false; return 0; }
    if (negative && magnitude == (UINT64_C(1)<<63)) { return INT64_MIN; }
    return negative ? -(int64_t)magnitude : (int64_t)magnitude;
}
static int64_t parse_factor(struct expression_parser *parser) {
    skip_space(parser); if (parser->depth >= 8) { parser->valid = false; return 0; } ++parser->depth;
    int64_t value;
    if (*parser->next == '(') { ++parser->next; value = parse_expression(parser); skip_space(parser); if (*parser->next != ')') { parser->valid = false; } else { ++parser->next; } }
    else if (*parser->next == '+' || *parser->next == '-') { bool negative = *parser->next++ == '-'; skip_space(parser); if (*parser->next >= '0' && *parser->next <= '9') { value = literal(parser,negative); } else { value = parse_factor(parser); if (negative && __builtin_sub_overflow((int64_t)0,value,&value)) { parser->valid = false; } } }
    else { value = literal(parser,false); }
    --parser->depth; return value;
}
static int64_t parse_term(struct expression_parser *parser) {
    int64_t value = parse_factor(parser); skip_space(parser);
    while (parser->valid && (*parser->next == '*' || *parser->next == '/')) { char operation = *parser->next++; int64_t right = parse_factor(parser); if (!parser->valid) { break; } if (operation == '*') { if (__builtin_mul_overflow(value,right,&value)) { parser->valid = false; } } else if (right == 0 || (value == INT64_MIN && right == -1)) { parser->valid = false; } else { value /= right; } skip_space(parser); } return value;
}
static int64_t parse_expression(struct expression_parser *parser) {
    int64_t value = parse_term(parser); skip_space(parser);
    while (parser->valid && (*parser->next == '+' || *parser->next == '-')) { char operation = *parser->next++; int64_t right = parse_term(parser); if (!parser->valid) { break; } if (operation == '+' ? __builtin_add_overflow(value,right,&value) : __builtin_sub_overflow(value,right,&value)) { parser->valid = false; } skip_space(parser); } return value;
}
bool wm_calculate(const char *expression, int64_t *result) { struct expression_parser parser = {.next = expression,.valid = true}; int64_t value = parse_expression(&parser); skip_space(&parser); if (!parser.valid || *parser.next != 0) { return false; } *result = value; return true; }
