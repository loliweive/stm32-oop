#include "mock_timer.h"
#include <string.h>

static MockTimerState *active = NULL;

static void _init(Timer *self)   { (void)self; if(active) active->initialized = true; }
static void _start(Timer *self)  { (void)self; if(active) active->running = true; }
static void _stop(Timer *self)   { (void)self; if(active) active->running = false; }

static void _set_period_us(Timer *self, uint32_t us) {
    (void)self; if(active) active->period_us = us;
}

static void _set_pwm(Timer *self, uint8_t ch, uint16_t duty) {
    (void)self; if(active && ch < 4) active->pwm_duty[ch] = duty;
}

static const TimerVtable _mock_vtable = {
    .init          = _init,
    .start         = _start,
    .stop          = _stop,
    .set_period_us = _set_period_us,
    .set_pwm       = _set_pwm,
};

void mock_timer_reset(MockTimerState *s) { memset(s, 0, sizeof(*s)); }
void mock_timer_inject(Timer *tim, MockTimerState *state) {
    tim->vtable = &_mock_vtable;
    active = state;
}
