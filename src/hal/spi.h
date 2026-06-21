/** OOP SPI master driver. */

#ifndef SPI_HAL_H
#define SPI_HAL_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    void     *spi;       /* SPI_Type * */
    uint32_t  speed_hz;
    uint32_t  pclk_hz;
    uint8_t   mode;      /* 0..3 (CPOL << 1 | CPHA) */
    uint8_t   _init;
} SpiPort;

void SpiPort_ctor(SpiPort *self, void *spi, uint32_t speed_hz, uint32_t pclk_hz, uint8_t mode);
void spi_init(SpiPort *self);
uint8_t spi_transfer(SpiPort *self, uint8_t byte);
void spi_transfer_buf(SpiPort *self, const uint8_t *tx, uint8_t *rx, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* SPI_HAL_H */
