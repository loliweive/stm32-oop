/**
 * @file    spi.c
 * @brief   SPI 主机驱动 — 使用 CMSIS 位定义
 */
#include "spi.h"
#include "stm32f1xx_hal.h"

void SpiPort_ctor(SpiPort *self, void *spi, uint32_t speed_hz, uint32_t pclk_hz, uint8_t mode)
{
    self->spi      = spi;
    self->speed_hz = speed_hz ? speed_hz : 1000000;
    self->pclk_hz  = pclk_hz  ? pclk_hz  : 36000000;
    self->mode     = mode;
    self->_init    = 0;
}

void spi_init(SpiPort *self)
{
    SPI_TypeDef *s = (SPI_TypeDef *)self->spi;

    /* 配置为主机模式 */
    uint16_t cr1 = SPI_CR1_MSTR  /* 主模式 */
                 | SPI_CR1_SSM   /* 软件 NSS */
                 | SPI_CR1_SSI;  /* 内部 NSS 高 */

    /* CPOL / CPHA */
    if (self->mode & 0x02) cr1 |= SPI_CR1_CPOL;
    if (self->mode & 0x01) cr1 |= SPI_CR1_CPHA;

    /* 波特率分频 = pclk / 2^(BR+1), 选择最小的满足 speed_hz 的分频 */
    uint32_t div = self->pclk_hz / self->speed_hz;
    uint8_t br = 0;
    while (div > (2UL << br) && br < 7) br++;
    cr1 |= (br << SPI_CR1_BR_Pos);

    s->CR1 = cr1;
    s->CR1 |= SPI_CR1_SPE;  /* 使能 SPI */
    self->_init = 1;
}

uint8_t spi_transfer(SpiPort *self, uint8_t byte)
{
    SPI_TypeDef *s = (SPI_TypeDef *)self->spi;

    /* 等待 TXE: 发送缓冲区为空 */
    while (!(s->SR & SPI_SR_TXE)) {}
    s->DR = byte;

    /* 等待 RXNE: 接收缓冲区非空 */
    while (!(s->SR & SPI_SR_RXNE)) {}
    return (uint8_t)s->DR;
}

void spi_transfer_buf(SpiPort *self, const uint8_t *tx, uint8_t *rx, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        uint8_t b = spi_transfer(self, tx ? tx[i] : 0xFF);
        if (rx) rx[i] = b;
    }
}
