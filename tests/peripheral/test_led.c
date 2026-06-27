/**
 * @brief  外设测试: LED (PC13) — 1Hz 闪烁 (SysTick 精确定时)
 * @usage  烧录后板载 LED 亮1秒灭1秒。正常=外设OK。
 */
#include "stm32f103xb.h"
#include "rcc.h"
#include "gpio.h"

/* SysTick 毫秒延时 (24-bit, 72MHz → 每 tick = 1/72μs) */
static void delay_ms(uint32_t ms) {
    /* 72MHz / 1000 = 72000 ticks/ms */
    SysTick->LOAD = 72000 - 1;
    SysTick->VAL  = 0;
    SysTick->CTRL = 5;  /* HCLK/1, no interrupt */
    while (ms--) {
        while (!(SysTick->CTRL & (1<<16))) {}  /* Wait COUNTFLAG */
    }
    SysTick->CTRL = 0;
}

int main(void) {
    rcc_set_sysclk(RCC_PLL, 9);
    rcc_enable_gpio('C');

    GpioPin led;
    GpioPin_ctor(&led, GPIOC, GPIO_PIN_13);
    gpio_set_mode(&led, GPIO_MODE_OUT_PP);
    gpio_set(&led, 1);  /* 灭 */

    while (1) {
        gpio_set(&led, 0);  delay_ms(1000);  /* 亮 1s */
        gpio_set(&led, 1);  delay_ms(1000);  /* 灭 1s */
    }
}
