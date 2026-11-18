#ifndef AGENTICOS_INTERRUPTS_H
#define AGENTICOS_INTERRUPTS_H
#include <stdint.h>
void interrupts_init(void);
void interrupts_self_test(void);
void interrupts_timer_test(void);
uint32_t interrupts_timer_start(void);
void interrupts_timer_stop(void);
uint64_t interrupts_ticks(void);
#endif
