/**
 * @brief  外设测试: SPI Flash W25Qxx (SPI1 PA5/6/7 + PB9 CS) — JEDEC ID
 * @usage  烧录后用逻辑分析仪或重新烧录 UART echo 查看结果。
 *         LED: 快闪3次=就绪, 慢闪=读ID失败, 常亮=读ID成功。
 *         正常=JEDEC ID 非 0x000000 且非 0xFFFFFF。
 */
#include "stm32f103xb.h"
#include "rcc.h"
#include "gpio.h"
#include "spi.h"

static GpioPin led, cs;

static uint8_t spi_xfer(uint8_t tx) {
    while (!(SPI1->SR & (1<<1))) {}  /* TXE */
    SPI1->DR = tx;
    while (!(SPI1->SR & (1<<0))) {}  /* RXNE */
    return (uint8_t)SPI1->DR;
}

int main(void) {
    rcc_set_sysclk(RCC_HSI, 0);
    rcc_enable_gpio('A'); rcc_enable_gpio('B'); rcc_enable_gpio('C');
    rcc_enable_spi(1);

    /* LED */
    GpioPin_ctor(&led, GPIOC, GPIO_PIN_13);
    gpio_set_mode(&led, GPIO_MODE_OUT_PP); gpio_set(&led, 1);

    /* SPI1 GPIO: PA5=SCK, PA6=MISO, PA7=MOSI */
    { GpioPin sck, miso, mosi;
      GpioPin_ctor(&sck,  GPIOA, GPIO_PIN_5); gpio_set_mode(&sck,  GPIO_CNF_ALT_PP | GPIO_MODE_OUT_50MHZ);
      GpioPin_ctor(&miso, GPIOA, GPIO_PIN_6); gpio_set_mode(&miso, GPIO_CNF_FLOAT | GPIO_MODE_IN);
      GpioPin_ctor(&mosi, GPIOA, GPIO_PIN_7); gpio_set_mode(&mosi, GPIO_CNF_ALT_PP | GPIO_MODE_OUT_50MHZ); }

    /* CS PB9 */
    GpioPin_ctor(&cs, GPIOB, GPIO_PIN_9);
    gpio_set_mode(&cs, GPIO_MODE_OUT_PP); gpio_set(&cs, 1);

    /* SPI1 init: Master, fPCLK/64 (~125kHz @ 8MHz), CPOL=0 CPHA=0 */
    SPI1->CR1 = (4<<3) | (1<<2) | (1<<6) | (1<<9);

    /* 启动: 3 快闪 */
    for (int i = 0; i < 3; i++) {
        gpio_set(&led, 0); for (volatile uint32_t j = 0; j < 100000; j++) __asm__("nop");
        gpio_set(&led, 1); for (volatile uint32_t j = 0; j < 100000; j++) __asm__("nop");
    }

    /* 读 JEDEC ID: CS↓ → 0x9F → 3 bytes → CS↑ */
    gpio_set(&cs, 0);
    spi_xfer(0x9F);
    uint8_t mfg = spi_xfer(0xFF);
    uint8_t mem = spi_xfer(0xFF);
    uint8_t cap = spi_xfer(0xFF);
    gpio_set(&cs, 1);

    /* 验证: 全0或全FF = 失败 */
    uint32_t id = ((uint32_t)mfg << 16) | ((uint32_t)mem << 8) | cap;
    if (id == 0x000000 || id == 0xFFFFFF) {
        /* 失败: 慢闪 */
        while (1) {
            gpio_set(&led, 0); for (volatile uint32_t j = 0; j < 800000; j++) __asm__("nop");
            gpio_set(&led, 1); for (volatile uint32_t j = 0; j < 800000; j++) __asm__("nop");
        }
    }

    /* 成功: 常亮 */
    gpio_set(&led, 0);
    while (1) {}
}
