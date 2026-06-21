/** OOP Timer/PWM driver. */

#ifndef TIMER_HAL_H
#define TIMER_HAL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Timer Timer;

typedef struct {
    void (*init)(Timer *self);
    void (*start)(Timer *self);
    void (*stop)(Timer *self);
    void (*set_period_us)(Timer *self, uint32_t us);
    void (*set_pwm)(Timer *self, uint8_t channel, uint16_t duty); /* 0..10000 = 0..100% */
} TimerVtable;

struct Timer {
    void            *tim;       /* TIM_Type * */
    const TimerVtable *vtable;
    uint32_t          pclk_hz;
    uint8_t           _init;
};

void Timer_ctor(Timer *self, void *tim, uint32_t pclk_hz);

static inline void timer_init(Timer *self)          { self->vtable->init(self); }
static inline void timer_start(Timer *self)         { self->vtable->start(self); }
static inline void timer_stop(Timer *self)          { self->vtable->stop(self); }
static inline void timer_set_period_us(Timer *self, uint32_t us) { self->vtable->set_period_us(self, us); }
static inline void timer_set_pwm(Timer *self, uint8_t ch, uint16_t duty) { self->vtable->set_pwm(self, ch, duty); }

#ifdef __cplusplus
}
#endif

#endif /* TIMER_HAL_H */
