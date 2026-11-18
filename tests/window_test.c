#include <assert.h>
#include <stdio.h>
#include <limits.h>
#include "window.h"
int main(void) {
    struct window_manager wm; wm_init(&wm,800,600);
    int a = wm_open(&wm,APP_TERMINAL), b = wm_open(&wm,APP_FILES), c = wm_open(&wm,APP_CALCULATOR);
    assert(a == 0 && b == 1 && c == 2 && wm.active == c);
    assert(wm_hit(&wm,500,250) == c); wm_focus(&wm,0); assert(wm_hit(&wm,400,250) == a);
    wm.dragging = 0; wm.anchor_x = 10; wm.anchor_y = 10; wm_drag(&wm,INT_MIN,INT_MIN,true); assert(wm.windows[0].x == 0 && wm.windows[0].y == 44);
    wm_drag(&wm,INT_MAX,INT_MAX,true); assert(wm.windows[0].x == 380 && wm.windows[0].y == 272);
    wm_drag(&wm,0,0,false); assert(wm.dragging == -1);
    wm_close(&wm,0); assert(wm.count == 2 && wm.active == c); wm_close(&wm,0); assert(wm.count == 2);
    for (unsigned int i = 0; i < 6; ++i) { assert(wm_open(&wm,APP_TERMINAL) >= 0); } assert(wm_open(&wm,APP_TERMINAL) == -1);
    int64_t result; assert(wm_calculate("6*7",&result) && result == 42); assert(wm_calculate("3 - 5",&result) && result == -2);
    assert(!wm_calculate("1/0",&result) && !wm_calculate("9223372036854775807+1",&result) && !wm_calculate("1+2oops",&result) && !wm_calculate("",&result));
    puts("PASS: window z-order/focus, drag clipping/release, close/reopen quotas and checked calculator operations"); return 0;
}
