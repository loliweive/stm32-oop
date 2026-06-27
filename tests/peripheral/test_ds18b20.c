/**
 * @brief  外设测试: DS18B20 温度传感器 (PA1 OneWire)
 * @usage  烧录后 LED 指示结果: 1快闪=就绪, 常亮=传感器OK, 慢闪=未检测到。
 *         正常=LED 常亮 (DS18B20 存在且可读取)。
 */
#include "stm32f103xb.h"
#include "rcc.h"
#include "gpio.h"
#include <stdbool.h>

static GpioPin led, dq;

/* OneWire 位操作 (8MHz HSI, 已校准) */
#define DQ_OUT()  gpio_set_mode(&dq, GPIO_MODE_OUT_PP)
#define DQ_IN()   gpio_set_mode(&dq, GPIO_CNF_PP | GPIO_MODE_IN); gpio_set(&dq, 1)
#define DQ_LOW()  gpio_set(&dq, 0)
#define DQ_HIGH() gpio_set(&dq, 1)
#define DQ_READ() (gpio_get(&dq))

static void ow_reset(void) {
    DQ_OUT(); DQ_LOW();
    for (volatile int i = 0; i < 600; i++) __asm__("nop");  /* 480μs */
    DQ_IN();
    for (volatile int i = 0; i < 100; i++) __asm__("nop");   /* 70μs */
    /* 检测 presence pulse */
    for (volatile int i = 0; i < 500; i++) __asm__("nop");   /* 410μs */
}

static void ow_write_bit(bool bit) {
    DQ_OUT(); DQ_LOW();
    if (bit) {
        for (volatile int i = 0; i < 3; i++) __asm__("nop");
        DQ_IN();
        for (volatile int i = 0; i < 70; i++) __asm__("nop");
    } else {
        for (volatile int i = 0; i < 75; i++) __asm__("nop");
        DQ_IN();
        for (volatile int i = 0; i < 3; i++) __asm__("nop");
    }
}

static bool ow_read_bit(void) {
    bool bit;
    DQ_OUT(); DQ_LOW();
    for (volatile int i = 0; i < 3; i++) __asm__("nop");
    DQ_IN();
    for (volatile int i = 0; i < 10; i++) __asm__("nop");
    bit = DQ_READ();
    for (volatile int i = 0; i < 60; i++) __asm__("nop");
    return bit;
}

static void ow_write_byte(uint8_t b) {
    for (int i = 0; i < 8; i++) { ow_write_bit(b & 1); b >>= 1; }
}

static uint8_t ow_read_byte(void) {
    uint8_t b = 0;
    for (int i = 0; i < 8; i++) { if (ow_read_bit()) b |= (1 << i); }
    return b;
}

static void delay_ms(uint32_t n) {
    for (uint32_t i = 0; i < n * 4000; i++) __asm__("nop");
}

int main(void) {
    rcc_set_sysclk(RCC_HSI, 0);
    rcc_enable_gpio('A'); rcc_enable_gpio('C');

    GpioPin_ctor(&led, GPIOC, GPIO_PIN_13);
    gpio_set_mode(&led, GPIO_MODE_OUT_PP); gpio_set(&led, 1);

    GpioPin_ctor(&dq, GPIOA, GPIO_PIN_1);
    DQ_IN();

    /* 1 快闪 = 就绪 */
    gpio_set(&led, 0); delay_ms(100); gpio_set(&led, 1); delay_ms(300);

    /* DS18B20 检测 */
    ow_reset();
    ow_write_byte(0xCC);  /* Skip ROM */
    ow_write_byte(0x44);  /* Convert T */
    delay_ms(800);

    ow_reset();
    ow_write_byte(0xCC);  /* Skip ROM */
    ow_write_byte(0xBE);  /* Read Scratchpad */

    uint8_t lsb = ow_read_byte();
    uint8_t msb = ow_read_byte();

    int16_t raw = (int16_t)((uint16_t)msb << 8) | lsb;
    /* 检查: 全0 (无设备) 或 全1 (总线错误) */
    if (raw == 0x0000 || raw == 0xFFFF || raw == 0x0550 /* 上电默认 85°C */) {
        /* 失败: 慢闪 */
        while (1) {
            gpio_set(&led, 0); delay_ms(500);
            gpio_set(&led, 1); delay_ms(500);
        }
    }

    /* 成功: 常亮 */
    gpio_set(&led, 0);
    while (1) {}
}
