/**
 * @file    spi.c
 * @brief   SPI 主机驱动 — OOP vtable 实现 (CMSIS 寄存器级, 带超时保护)
 */
#include "spi.h"
#include "stm32f103xb.h"

/* ── 超时常量 ────────────────────────────────────────────────── */
#define SPI_TIMEOUT  100000U  /* TXE/RXNE 最大等待周期数 */

/* ── 生产 vtable 函数 ────────────────────────────────────────── */

static void _spi_init(SpiPort *self)
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

static uint8_t _spi_transfer(SpiPort *self, uint8_t byte)
{
    SPI_TypeDef *s = (SPI_TypeDef *)self->spi;
    uint32_t to;

    /* 等待 TXE: 发送缓冲区为空 (带超时保护) */
    to = SPI_TIMEOUT;
    while (!(s->SR & SPI_SR_TXE) && --to) {}
    if (to == 0) {
        /* TXE 超时: 禁用 SPI, 清 OVR, 重新使能 */
        s->CR1 &= ~SPI_CR1_SPE;
        (void)s->SR; (void)s->DR;  /* 读 SR+DR 清 OVR */
        s->CR1 |= SPI_CR1_SPE;
        return 0xFF;
    }
    s->DR = byte;

    /* 等待 RXNE: 接收缓冲区非空 (带超时保护) */
    to = SPI_TIMEOUT;
    while (!(s->SR & SPI_SR_RXNE) && --to) {}
    if (to == 0) {
        s->CR1 &= ~SPI_CR1_SPE;
        (void)s->SR; (void)s->DR;
        s->CR1 |= SPI_CR1_SPE;
        return 0xFF;
    }
    return (uint8_t)s->DR;
}

static void _spi_transfer_buf(SpiPort *self, const uint8_t *tx, uint8_t *rx, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        uint8_t b = spi_transfer(self, tx ? tx[i] : 0xFF);
        if (rx) rx[i] = b;
    }
}

/* ── Vtable 实例 ─────────────────────────────────────────────── */

static const SpiVtable _spi_vtable = {
    .init         = _spi_init,
    .transfer     = _spi_transfer,
    .transfer_buf = _spi_transfer_buf,
};

/* ── 构造函数 ────────────────────────────────────────────────── */

void SpiPort_ctor(SpiPort *self, void *spi, uint32_t speed_hz, uint32_t pclk_hz, uint8_t mode)
{
    self->spi      = spi;
    self->vtable   = &_spi_vtable;
    self->speed_hz = speed_hz ? speed_hz : 1000000;
    self->pclk_hz  = pclk_hz  ? pclk_hz  : 36000000;
    self->mode     = mode;
    self->_init    = 0;
}
