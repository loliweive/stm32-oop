/**
 * @brief  外设测试: 按键 (PB14) — LED 随按键变化
 * @usage  按 PB14 → LED 亮, 松开 → LED 灭。正常=按键控制LED。
 */
#include "stm32f103xb.h"
#include "rcc.h"
#include "gpio.h"

int main(void) {
    rcc_set_sysclk(RCC_HSI, 0);
    rcc_enable_gpio('B'); rcc_enable_gpio('C');

    GpioPin led, btn;
    GpioPin_ctor(&led, GPIOC, GPIO_PIN_13);
    gpio_set_mode(&led, GPIO_MODE_OUT_PP);
    gpio_set(&led, 1);

    GpioPin_ctor(&btn, GPIOB, GPIO_PIN_14);
    gpio_set_mode(&btn, GPIO_CNF_PP | GPIO_MODE_IN);
    gpio_set(&btn, 1);  /* 内部上拉 */

    /* 启动闪烁: 3 快闪 = 就绪 */
    for (int i = 0; i < 3; i++) {
        gpio_set(&led, 0); for (volatile uint32_t j = 0; j < 100000; j++) __asm__("nop");
        gpio_set(&led, 1); for (volatile uint32_t j = 0; j < 100000; j++) __asm__("nop");
    }

    while (1) {
        uint8_t pressed = (gpio_get(&btn) == 0);  /* PB14: 低=按下 */
        gpio_set(&led, pressed ? 0 : 1);           /* 按下亮, 松开灭 */
    }
}
