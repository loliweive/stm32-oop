#include "delay.h"
#include "core_cm3.h"

static volatile uint32_t _ticks;

void delay_init(void)
{
    /* Configure SysTick for 1ms period at 72MHz: 72000 - 1 */
    SysTick->LOAD = 72000 - 1;
    SysTick->VAL  = 0;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk
                  | SysTick_CTRL_TICKINT_Msk
                  | SysTick_CTRL_ENABLE_Msk;
    _ticks = 0;
}

void delay_ms(uint32_t ms)
{
    uint32_t start = _ticks;
    while ((_ticks - start) < ms) {
        __WFI(); /* Sleep until next interrupt */
    }
}

void delay_us(uint32_t us)
{
    /* Rough busy-loop: ~12 cycles per iteration at 72MHz → ~167ns/iter */
    uint32_t count = us * 6;
    while (count--) {
        __NOP();
    }
}

uint32_t millis(void)
{
    return _ticks;
}

/* SysTick ISR */
void SysTick_Handler(void)
{
    _ticks++;
}
