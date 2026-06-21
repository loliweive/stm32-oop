/** OOP I2C master driver. */

#ifndef I2C_HAL_H
#define I2C_HAL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    void    *i2c;       /* I2C_Type * */
    uint32_t speed_hz;  /* 100000 or 400000 typical */
    uint32_t pclk_hz;   /* APB1 clock */
    bool     _init;
} I2cPort;

void I2cPort_ctor(I2cPort *self, void *i2c, uint32_t speed_hz, uint32_t pclk_hz);
void i2c_init(I2cPort *self);
bool i2c_write(I2cPort *self, uint8_t addr, const uint8_t *data, size_t len);
bool i2c_read(I2cPort *self, uint8_t addr, uint8_t *data, size_t len);
bool i2c_write_reg(I2cPort *self, uint8_t addr, uint8_t reg, uint8_t val);
bool i2c_read_reg(I2cPort *self, uint8_t addr, uint8_t reg, uint8_t *val);

#ifdef __cplusplus
}
#endif

#endif /* I2C_HAL_H */
