/**
 * Breathing LED — PC13 PWM fade via TIM4.
 *
 * Build & flash:
 *   cmake -B build/target -DBUILD_MODE=stm32f103
 *   cmake --build build/target
 *   openocd -f interface/stlink.cfg -f target/stm32f1x.cfg -c "program build/target/stm32-oop.elf verify reset exit"
 *
 * To switch from blinky to breathing, change main.c to call breathing_main().
 */
#include "stm32f103xb.h"
#include "rcc.h"
#include "gpio.h"
#include "timer.h"

/* Use a simple software fade since TIM PWM remap on PC13 requires AFIO config.
   For a cleaner hardware PWM demo, wire a LED to PA0 (TIM2_CH1). */
static void soft_pwm_fade(GpioPin *led)
{
    static int brightness = 0;
    static int direction = 1;

    /* Rough software PWM: 100 steps */
    for (int i = 0; i < 100; i++) {
        gpio_set(led, i < brightness ? 1 : 0);
        for (volatile int d = 0; d < 50; d++) { __NOP(); }
    }

    brightness += direction;
    if (brightness >= 100) { direction = -1; }
    if (brightness <= 0)   { direction = 1; }
}

int breathing_main(void)
{
    rcc_set_sysclk(RCC_HSE, 9);
    rcc_enable_gpio('C');

    GpioPin led;
    GpioPin_ctor(&led, GPIOC, GPIO_PIN_13);
    gpio_set_mode(&led, GPIO_MODE_OUT_PP);

    while (1) {
        soft_pwm_fade(&led);
    }
    return 0;
}
