/**
 * @brief  外设测试: LED (PC13) — 闪烁
 * @usage  烧录后板载 LED 以 1Hz 闪烁。正常=外设OK。
 */
#include "stm32f103xb.h"
#include "rcc.h"
#include "gpio.h"

int main(void) {
    rcc_set_sysclk(RCC_HSI, 0);
    rcc_enable_gpio('C');

    GpioPin led;
    GpioPin_ctor(&led, GPIOC, GPIO_PIN_13);
    gpio_set_mode(&led, GPIO_MODE_OUT_PP);
    gpio_set(&led, 1);  /* 灭 */

    while (1) {
        gpio_set(&led, 0);  /* 亮 */
        for (volatile uint32_t i = 0; i < 500000; i++) __asm__("nop");
        gpio_set(&led, 1);  /* 灭 */
        for (volatile uint32_t i = 0; i < 500000; i++) __asm__("nop");
    }
}
