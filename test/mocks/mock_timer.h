/** Mock Timer for host-side TDD. */

#ifndef MOCK_TIMER_H
#define MOCK_TIMER_H

#include "timer.h"
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    bool     initialized;
    bool     running;
    uint32_t period_us;
    uint16_t pwm_duty[4];
    uint32_t pclk_hz;
} MockTimerState;

void mock_timer_reset(MockTimerState *s);
void mock_timer_inject(Timer *tim, MockTimerState *state);

#endif
