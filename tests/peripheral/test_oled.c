/**
 * @brief  外设测试: SSD1306 OLED (I2C1, PB6/PB7) — 全屏测试图案
 * @usage  烧录后 OLED 依次显示: 文本 → 几何图形 → 计数器。正常=每屏清晰无噪点。
 */
#include "stm32f103xb.h"
#include "rcc.h"
#include "gpio.h"
#include "i2c.h"
#include "ssd1306.h"
#include "oled.h"

/* SysTick 毫秒延时 */
static void delay_ms(uint32_t ms) {
    while (ms--) {
        while (!(SysTick->CTRL & (1<<16))) {}  /* Wait COUNTFLAG = 1ms */
    }
}

int main(void) {
    rcc_set_sysclk(RCC_PLL, 9);
    /* SysTick 1ms tick */
    SysTick->LOAD = 72000 - 1;
    SysTick->VAL  = 0;
    SysTick->CTRL = 5;

    rcc_enable_gpio('B'); rcc_enable_gpio('C'); rcc_enable_i2c(1);

    /* LED PC13 */
    GpioPin led; GpioPin_ctor(&led, GPIOC, GPIO_PIN_13);
    gpio_set_mode(&led, GPIO_MODE_OUT_PP); gpio_set(&led, 1);

    /* I2C1 GPIO */
    { GpioPin scl, sda;
      GpioPin_ctor(&scl, GPIOB, GPIO_PIN_6); gpio_set_mode(&scl, GPIO_CNF_ALT_OD | GPIO_MODE_OUT_50MHZ);
      GpioPin_ctor(&sda, GPIOB, GPIO_PIN_7); gpio_set_mode(&sda, GPIO_CNF_ALT_OD | GPIO_MODE_OUT_50MHZ); }

    I2cPort ip; I2cPort_ctor(&ip, I2C1, 400000, 36000000); i2c_init(&ip);  /* APB1=36MHz @72MHz */

    /* OLED */
    SSD1306 oled; ssd1306_ctor(&oled, I2C1, 0x3C);
    OledDisplay *d = &oled.base;
    oled_init(d);

    /* 1: 文本 (2秒) */
    oled_clear(d);
    oled_show_string(d, 0,  0, "OLED Test", OLED_FONT_8X16);
    oled_show_string(d, 0, 16, "SSD1306 OK", OLED_FONT_8X16);
    oled_show_string(d, 0, 40, "I2C1 PB6/PB7", OLED_FONT_6X8);
    oled_flush(d); delay_ms(2000);

    /* 2: 几何 (2秒) */
    oled_clear(d);
    oled_draw_line(d, 0, 31, 127, 31);
    oled_draw_line(d, 63, 0, 63, 63);
    oled_draw_rect(d, 4, 4, 20, 12, OLED_UNFILLED);
    oled_draw_rect(d, 28, 4, 20, 12, OLED_FILLED);
    oled_draw_circle(d, 74, 10, 8, OLED_UNFILLED);
    oled_draw_circle(d, 100, 10, 8, OLED_FILLED);
    oled_draw_triangle(d, 4, 42, 22, 42, 13, 60, OLED_UNFILLED);
    oled_flush(d); delay_ms(2000);

    /* 3: 计数器 (每秒递增) */
    int cnt = 0;
    while (1) {
        oled_clear(d);
        oled_show_string(d, 0, 8, "Counter:", OLED_FONT_8X16);
        oled_show_num(d, 0, 28, cnt, 6, OLED_FONT_8X16);
        oled_show_hex(d, 0, 48, cnt, 8, OLED_FONT_6X8);
        oled_flush(d);
        gpio_set(&led, 0); delay_ms(100); gpio_set(&led, 1);
        delay_ms(900);
        cnt++;
    }
}
