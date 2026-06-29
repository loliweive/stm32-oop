#include "timer.h"
#include "stm32f1xx_hal.h"

static void _init(Timer *self);
static void _start(Timer *self);
static void _stop(Timer *self);
static void _set_period_us(Timer *self, uint32_t us);
static void _set_pwm(Timer *self, uint8_t channel, uint16_t duty);

static const TimerVtable _timer_vtable = {
    .init          = _init,
    .start         = _start,
    .stop          = _stop,
    .set_period_us = _set_period_us,
    .set_pwm       = _set_pwm,
};

void Timer_ctor(Timer *self, void *tim, uint32_t pclk_hz)
{
    self->tim     = tim;
    self->vtable  = &_timer_vtable;
    self->pclk_hz = pclk_hz;
    self->_init   = 0;
}

static void _init(Timer *self)
{
    TIM_TypeDef *t = (TIM_TypeDef *)self->tim;
    t->CR1 |= TIM_CR1_ARPE;   /* Auto-reload preload enable */
    self->_init = 1;
}

static void _start(Timer *self)
{
    TIM_TypeDef *t = (TIM_TypeDef *)self->tim;
    t->CR1 |= TIM_CR1_CEN;
}

static void _stop(Timer *self)
{
    TIM_TypeDef *t = (TIM_TypeDef *)self->tim;
    t->CR1 &= ~TIM_CR1_CEN;
}

static void _set_period_us(Timer *self, uint32_t us)
{
    TIM_TypeDef *t = (TIM_TypeDef *)self->tim;

    /* 守卫: us=0 会导致 ARR 下溢为 0xFFFFFFFF */
    if (us == 0) return;

    /* Timer tick = pclk / (PSC + 1). Target: 1MHz for µs resolution.
       如果 pclk < 1MHz, PSC 会下溢 — 加守卫保护 */
    uint32_t psc = (self->pclk_hz >= 1000000)
                   ? (self->pclk_hz / 1000000) - 1
                   : 0;
    t->PSC = psc;
    t->ARR = us - 1;
    t->EGR = 0x01;  /* UG: Update generation — 将 PSC/ARR 加载到影子寄存器 */
}

static void _set_pwm(Timer *self, uint8_t channel, uint16_t duty)
{
    TIM_TypeDef *t = (TIM_TypeDef *)self->tim;
    if (channel < 4) {
        /* duty: 0..10000 maps to 0..ARR */
        uint32_t ccr = ((uint32_t)duty * (t->ARR + 1)) / 10000;
        t->CCR1 = ccr;

        /* Configure channel for PWM mode 1 */
        uint32_t shift = channel * 8;
        if (channel < 2) {
            t->CCMR1 = (t->CCMR1 & ~(0xFFUL << shift)) | (0x60UL << shift); /* PWM1 */
        } else {
            t->CCMR2 = (t->CCMR2 & ~(0xFFUL << ((channel - 2) * 8))) | (0x60UL << ((channel - 2) * 8));
        }
        t->CCER |= (1UL << (channel * 4)); /* CCxE enable */
    }
}
