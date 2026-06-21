#include "spi.h"
#include "stm32f103xb.h"

void SpiPort_ctor(SpiPort *self, void *spi, uint32_t speed_hz, uint32_t pclk_hz, uint8_t mode)
{
    self->spi      = spi;
    self->speed_hz = speed_hz;
    self->pclk_hz  = pclk_hz;
    self->mode     = mode;
    self->_init    = 0;
}

void spi_init(SpiPort *self)
{
    SPI_Type *s = (SPI_Type *)self->spi;

    /* Configure as master */
    uint16_t cr1 = 0x0004;  /* MSTR */
    cr1 |= ((self->mode & 0x02) ? 0x0002 : 0);  /* CPOL */
    cr1 |= ((self->mode & 0x01) ? 0x0001 : 0);  /* CPHA */

    /* Baud rate: pclk / 2^(n+1), where n is BR[2:0] */
    uint32_t div = self->pclk_hz / self->speed_hz;
    uint8_t br = 0;
    while (div > (2UL << br) && br < 7) br++;
    cr1 |= (br << 3);

    s->CR1 = cr1;
    s->CR1 |= 0x0040;  /* SPE: enable */
    self->_init = 1;
}

uint8_t spi_transfer(SpiPort *self, uint8_t byte)
{
    SPI_Type *s = (SPI_Type *)self->spi;

    /* Wait TXE */
    while (!(s->SR & 0x02)) {}
    s->DR = byte;

    /* Wait RXNE */
    while (!(s->SR & 0x01)) {}
    return (uint8_t)s->DR;
}

void spi_transfer_buf(SpiPort *self, const uint8_t *tx, uint8_t *rx, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        uint8_t b = spi_transfer(self, tx ? tx[i] : 0xFF);
        if (rx) rx[i] = b;
    }
}
