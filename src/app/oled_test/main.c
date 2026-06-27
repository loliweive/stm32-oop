/**
 * @file    main.c
 * @brief   OLED 测试 — OOP OLED API 演示
 *
 * 使用 SSD1306 OOP 驱动演示:
 *   - 初始化 (oled_init)
 *   - 字符串显示 (oled_show_string)
 *   - 数值显示 (oled_show_num)
 *   - 几何绘图 (oled_draw_*)
 *   - 帧缓冲刷新 (oled_flush)
 *
 * 接线:
 *   SCL → PB6, SDA → PB7 (I2C1)
 *   板载 LED → PC13
 */

#include "stm32f103xb.h"
#include "rcc.h"
#include "gpio.h"
#include "i2c.h"
#include "ssd1306.h"

/* ── LED (PC13, 低电平亮) ────────────────────────────── */
static GpioPin _led;

static void led_on(void)  { gpio_set(&_led, 0); }
static void led_off(void) { gpio_set(&_led, 1); }
static void delay_ms(uint32_t n) {
    for (uint32_t i = 0; i < n * 4000; i++) { __asm__("nop"); }
}

/* ── 入口 ────────────────────────────────────────────── */
int main(void)
{
    /* 时钟 & GPIO */
    rcc_set_sysclk(RCC_HSI, 0);
    rcc_enable_gpio('B');
    rcc_enable_gpio('C');
    rcc_enable_i2c(1);

    /* 板载 LED */
    GpioPin_ctor(&_led, GPIOC, GPIO_PIN_13);
    gpio_set_mode(&_led, GPIO_MODE_OUT_PP);
    led_off();

    /* I2C1 GPIO: PB6=SCL, PB7=SDA (复用开漏) */
    {
        GpioPin scl, sda;
        GpioPin_ctor(&scl, GPIOB, GPIO_PIN_6);
        gpio_set_mode(&scl, GPIO_CNF_ALT_OD | GPIO_MODE_OUT_50MHZ);
        GpioPin_ctor(&sda, GPIOB, GPIO_PIN_7);
        gpio_set_mode(&sda, GPIO_CNF_ALT_OD | GPIO_MODE_OUT_50MHZ);
    }

    /* I2C 初始化 */
    {
        I2cPort p;
        I2cPort_ctor(&p, I2C1, 400000, 8000000);
        i2c_init(&p);
    }

    /* 启动闪烁 — 快闪 2 次 = 启动 */
    led_on();  delay_ms(100); led_off(); delay_ms(100);
    led_on();  delay_ms(100); led_off(); delay_ms(100);

    /* ═══════════════════════════════════════════════════
     * OOP OLED 初始化
     * ═══════════════════════════════════════════════════ */

    SSD1306 oled;
    ssd1306_ctor(&oled, I2C1, 0x3C);
    OledDisplay *d = &oled.base;

    oled_init(d);   /* 发送初始化序列 + 清屏 */
    led_on();  delay_ms(300); led_off(); /* 慢闪 1 次 = OLED 就绪 */

    /* ── 第 1 屏：文本演示 ─────────────────────────────
     * 8x16 每行 16px → Y=0, 16, 32, 48
     * 6x8  每行 8px  → Y=0, 8, 16, 24, 32, 40, 48, 56 */
    oled_show_string(d, 0,  0, "STM32F103", OLED_FONT_8X16);     /* 行1, 16px */
    oled_show_string(d, 0, 16, "SSD1306 OLED", OLED_FONT_8X16);  /* 行2, 16px */
    oled_show_string(d, 0, 48, "Hello World!", OLED_FONT_6X8);   /* 行7, 8px */
    oled_flush(d);
    delay_ms(2000);

    /* ── 第 2 屏：中文 + 数值 ─────────────────────────── */
    oled_clear(d);
    oled_show_string(d, 0,  0, "你好世界", OLED_FONT_8X16);      /* 16px */
    oled_show_string(d, 0, 16, "Temp:", OLED_FONT_8X16);         /* 16px */
    oled_show_float(d, 40, 16, 26.5, 2, 1, OLED_FONT_8X16);     /* 同行 */
    oled_show_string(d, 0, 40, "Hum:", OLED_FONT_6X8);           /* 8px */
    oled_show_num(d, 30, 40, 58, 2, OLED_FONT_6X8);              /* 同行 */
    oled_show_string(d, 44, 40, "%", OLED_FONT_6X8);              /* 同行 */
    oled_flush(d);
    delay_ms(2000);

    /* ── 第 3 屏：几何绘图 ───────────────────────────── */
    oled_clear(d);
    /* 十字线 */
    oled_draw_line(d, 0, 31, 127, 31);
    oled_draw_line(d, 63, 0, 63, 63);
    /* 矩形 */
    oled_draw_rect(d, 4, 4, 20, 12, OLED_UNFILLED);
    oled_draw_rect(d, 28, 4, 20, 12, OLED_FILLED);
    /* 圆 */
    oled_draw_circle(d, 74, 10, 8, OLED_UNFILLED);
    oled_draw_circle(d, 100, 10, 8, OLED_FILLED);
    /* 三角形 */
    oled_draw_triangle(d, 4, 40, 20, 40, 12, 58, OLED_UNFILLED);
    oled_draw_triangle(d, 28, 40, 44, 40, 36, 58, OLED_FILLED);
    oled_flush(d);
    delay_ms(2000);

    /* ── 第 4 屏：动态计数器 ─────────────────────────── */
    int cnt = 0;
    while (1) {
        oled_clear(d);
        oled_show_string(d, 0,  8, "Counter:", OLED_FONT_8X16);  /* 16px, 居中偏上 */
        oled_show_num(d,     0, 28, cnt, 6, OLED_FONT_8X16);     /* 16px 数字 */
        oled_show_hex(d,     0, 48, cnt, 8, OLED_FONT_6X8);      /* 8px, hex 辅助 */
        oled_flush(d);

        led_on();  delay_ms(50); led_off();
        delay_ms(950);
        cnt++;
    }
}
