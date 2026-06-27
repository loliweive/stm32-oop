/**
 * @brief  外设测试: SPI Flash W25Qxx (SPI1 PA5/6/7 + PB9 CS)
 * @usage  LED 常亮=JEDEC ID OK, LED 慢闪=未检测到
 */
#include "stm32f103xb.h"
#include "rcc.h"
#include "gpio.h"

static GpioPin led, cs;

/* SysTick 1ms delay */
static void delay_ms(uint32_t ms) {
    while (ms--) while (!(SysTick->CTRL & (1<<16))) {}
}

static uint8_t spi_xfer(uint8_t tx) {
    while (!(SPI1->SR & (1<<1))) {}  /* TXE */
    SPI1->DR = tx;
    while (!(SPI1->SR & (1<<0))) {}  /* RXNE */
    return (uint8_t)SPI1->DR;
}

int main(void) {
    rcc_set_sysclk(RCC_PLL, 9);
    SysTick->LOAD = 72000 - 1; SysTick->VAL = 0; SysTick->CTRL = 5;

    rcc_enable_gpio('A'); rcc_enable_gpio('B'); rcc_enable_gpio('C');
    rcc_enable_spi(1);

    /* LED */
    GpioPin_ctor(&led, GPIOC, GPIO_PIN_13);
    gpio_set_mode(&led, GPIO_MODE_OUT_PP); gpio_set(&led, 1);

    /* SPI1 GPIO */
    { GpioPin sck, miso, mosi;
      GpioPin_ctor(&sck,  GPIOA, GPIO_PIN_5); gpio_set_mode(&sck,  GPIO_CNF_ALT_PP | GPIO_MODE_OUT_50MHZ);
      GpioPin_ctor(&miso, GPIOA, GPIO_PIN_6); gpio_set_mode(&miso, GPIO_CNF_FLOAT | GPIO_MODE_IN);
      GpioPin_ctor(&mosi, GPIOA, GPIO_PIN_7); gpio_set_mode(&mosi, GPIO_CNF_ALT_PP | GPIO_MODE_OUT_50MHZ); }

    /* CS PB9 */
    GpioPin_ctor(&cs, GPIOB, GPIO_PIN_9);
    gpio_set_mode(&cs, GPIO_MODE_OUT_PP); gpio_set(&cs, 1);

    /* SPI1: Master, fPCLK/256 (~281kHz), SW slave mgmt, MSB first */
    SPI1->CR1 = (7<<3) | (1<<2) | (1<<8) | (1<<9) | (1<<6);

    /* 启动: 3 慢闪 (~500ms each) */
    for (int i = 0; i < 3; i++) {
        gpio_set(&led, 0); delay_ms(200);
        gpio_set(&led, 1); delay_ms(200);
    }

    /* 读 JEDEC ID */
    gpio_set(&cs, 0);
    for (volatile int i = 0; i < 10; i++) __asm__("nop");
    spi_xfer(0x9F);
    uint8_t mfg = spi_xfer(0xFF);
    uint8_t mem = spi_xfer(0xFF);
    uint8_t cap = spi_xfer(0xFF);
    gpio_set(&cs, 1);

    uint32_t id = ((uint32_t)mfg << 16) | ((uint32_t)mem << 8) | cap;
    if (id == 0x000000 || id == 0xFFFFFF) {
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
