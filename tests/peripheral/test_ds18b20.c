/**
 * @brief  外设测试: DS18B20 (PA1 OneWire) @72MHz — DWT 精确定时
 * @usage  LED 1快闪=就绪, 常亮=传感器OK, 慢闪=未检测到
 */
#include "stm32f103xb.h"
#include "rcc.h"
#include "gpio.h"
#include <stdbool.h>

/* DWT (Data Watchpoint and Trace) — not in stm32f103xb.h */
typedef struct { volatile uint32_t CTRL, CYCCNT; } DWT_Type;
#define DWT ((DWT_Type *)0xE0001000UL)
#define DWT_CTRL_CYCCNTENA (1<<0)
#define CoreDebug_DEMCR_TRCENA (1<<24)
#define CoreDebug_DEMCR (*(volatile uint32_t *)0xE000EDFCUL)

static GpioPin led, dq;

/* SysTick ms delay */
static void delay_ms(uint32_t ms) {
    while (ms--) while (!(SysTick->CTRL & (1<<16))) {}
}

/* DWT µs delay (Cortex-M3 cycle counter, 72MHz) */
static void delay_us(uint32_t us) {
    uint32_t start = DWT->CYCCNT;
    uint32_t ticks = us * 72;  /* 72 cycles/µs @72MHz */
    while ((DWT->CYCCNT - start) < ticks) {}
}

/* OneWire bit-bang */
#define DQ_OUT()  gpio_set_mode(&dq, GPIO_MODE_OUT_PP)
#define DQ_IN()   gpio_set_mode(&dq, 0x8 | GPIO_MODE_IN); gpio_set(&dq, 1)
#define DQ_LOW()  gpio_set(&dq, 0)
#define DQ_HIGH() gpio_set(&dq, 1)
#define DQ_READ() (gpio_get(&dq))

static bool ow_reset(void) {
    bool present;
    DQ_OUT(); DQ_LOW();  delay_us(500);           /* 500µs reset pulse */
    DQ_IN();             delay_us(70);             /* release + wait */
    present = (DQ_READ() == 0);                    /* sample presence */
    delay_us(420);                                 /* complete timeslot */
    return present;
}

static void ow_write_bit(bool bit) {
    DQ_OUT(); DQ_LOW();  delay_us(2);             /* start timeslot */
    if (bit) {
        DQ_IN();         delay_us(62);             /* release, bus pulled high */
    } else {
        delay_us(62);                              /* hold low for 64µs total */
        DQ_IN();         delay_us(2);
    }
}

static bool ow_read_bit(void) {
    bool bit;
    DQ_OUT(); DQ_LOW();  delay_us(2);             /* start timeslot */
    DQ_IN();             delay_us(12);             /* let bus settle */
    bit = DQ_READ();                               /* sample */
    delay_us(52);                                  /* complete timeslot */
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

int main(void) {
    rcc_set_sysclk(RCC_PLL, 9);
    /* SysTick 1ms */
    SysTick->LOAD = 72000 - 1; SysTick->VAL = 0; SysTick->CTRL = 5;
    /* DWT cycle counter */
    CoreDebug_DEMCR |= CoreDebug_DEMCR_TRCENA;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA;

    rcc_enable_gpio('A'); rcc_enable_gpio('C');

    GpioPin_ctor(&led, GPIOC, GPIO_PIN_13);
    gpio_set_mode(&led, GPIO_MODE_OUT_PP); gpio_set(&led, 1);

    GpioPin_ctor(&dq, GPIOA, GPIO_PIN_1);
    DQ_IN();

    /* 1 快闪 = 就绪 */
    gpio_set(&led, 0); delay_ms(200); gpio_set(&led, 1); delay_ms(500);

    /* DS18B20: Convert T */
    ow_reset();
    ow_write_byte(0xCC); ow_write_byte(0x44);
    delay_ms(800);

    /* Read Scratchpad */
    ow_reset();
    ow_write_byte(0xCC); ow_write_byte(0xBE);
    uint8_t lsb = ow_read_byte();
    uint8_t msb = ow_read_byte();

    int16_t raw = (int16_t)((uint16_t)msb << 8) | lsb;
    if (raw == 0x0000 || raw == 0xFFFF || raw == 0x0550) {
        while (1) { gpio_set(&led, 0); delay_ms(500); gpio_set(&led, 1); delay_ms(500); }
    }

    gpio_set(&led, 0);  /* OK */
    while (1) {}
}
