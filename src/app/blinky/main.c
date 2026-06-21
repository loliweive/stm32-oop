/**
 * Blinky — LED on PC13 (built-in on Blue Pill boards).
 *
 * First experiment: validates the full toolchain:
 *   cmake -B build/target -DTARGET=stm32f103
 *   cmake --build build/target
 *   openocd -f interface/stlink.cfg -f target/stm32f1x.cfg -c "program build/target/stm32-oop.elf verify reset exit"
 *
 * Expected: LED blinks at 1Hz.
 */

#include "stm32f103xb.h"
#include "rcc.h"
#include "gpio.h"

static void dumb_delay(uint32_t count)
{
    while (count--) {
        __NOP();
    }
}

int main(void)
{
    /* Configure clock to 72MHz via HSE+PLL */
    rcc_set_sysclk(RCC_HSE, 9);

    /* Enable GPIOC clock */
    rcc_enable_gpio('C');

    /* PC13 — Blue Pill onboard LED */
    GpioPin led;
    GpioPin_ctor(&led, GPIOC, GPIO_PIN_13);
    gpio_set_mode(&led, GPIO_MODE_OUT_PP);

    while (1) {
        gpio_toggle(&led);
        dumb_delay(4000000); /* ~500ms at 72MHz */
    }

    return 0;
}
