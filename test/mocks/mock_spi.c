/**
 * @file    mock_spi.c
 * @brief   SPI + GPIO Mock 构造函数 — 链接时替换 spi.c + gpio.c
 *
 * 提供 mock SpiPort_ctor 和 GpioPin_ctor, 使 spi_flash_init 内部
 * 的构造调用直接绑定 mock vtable, 无需在测试中手动注入。
 */
#include "mock_spi.h"
#include "mock_gpio.h"
#include <string.h>

static MockSpiState *active = NULL;

/* ── Mock vtable 函数 ────────────────────────────────────────── */

static void _mock_init(SpiPort *self)
{
    (void)self;
    if (!active) return;
    active->initialized = true;
}

static uint8_t _mock_transfer(SpiPort *self, uint8_t byte)
{
    (void)self;
    if (!active) return 0x00;  /* 0x00 = not busy, safe default */

    /* 记录发送的字节 */
    if (active->tx_count < 128)
        active->tx_log[active->tx_count++] = byte;

    active->transfer_count++;

    /* 返回预设响应 (循环) */
    if (active->rx_len > 0) {
        uint8_t r = active->rx_data[active->rx_idx % active->rx_len];
        active->rx_idx++;
        return r;
    }
    return 0x00;  /* 0x00 = not busy, safe default */
}

static void _mock_transfer_buf(SpiPort *self, const uint8_t *tx, uint8_t *rx, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        uint8_t b = _mock_transfer(self, tx ? tx[i] : 0xFF);
        if (rx) rx[i] = b;
    }
}

/* ── Mock vtable 实例 ────────────────────────────────────────── */

static const SpiVtable _mock_spi_vtable = {
    .init         = _mock_init,
    .transfer     = _mock_transfer,
    .transfer_buf = _mock_transfer_buf,
};

/* ── 公共接口 ────────────────────────────────────────────────── */

void mock_spi_reset(MockSpiState *s)
{
    memset(s, 0, sizeof(*s));
}

void mock_spi_inject(SpiPort *spi, MockSpiState *state)
{
    spi->vtable = &_mock_spi_vtable;
    active = state;
}

void mock_spi_set_global(MockSpiState *s)
{
    active = s;
}

/* ── Mock 构造函数 (替换 spi.c + gpio.c 的版本) ───────────── */
/*    链接时 mock_spi.c 替代真实的 spi.c — 以下构造函数确保    */
/*    spi_flash_init 内部的 SpiPort_ctor / GpioPin_ctor 自动    */
/*    绑定 mock vtable, 不会访问真实硬件寄存器。                 */

void SpiPort_ctor(SpiPort *self, void *spi, uint32_t speed_hz,
                  uint32_t pclk_hz, uint8_t mode)
{
    self->spi      = spi;
    self->vtable   = &_mock_spi_vtable;
    self->speed_hz = speed_hz ? speed_hz : 1000000;
    self->pclk_hz  = pclk_hz  ? pclk_hz  : 36000000;
    self->mode     = mode;
    self->_init    = 0;
}

void GpioPin_ctor(GpioPin *self, void *port, uint16_t pin)
{
    self->port   = port;
    self->pin    = pin;
    self->mode   = 0;
    self->vtable = mock_gpio_get_vtable();
    self->_init  = 0;
}
