/** OOP I2C master driver — HAL API internally */

#ifndef I2C_HAL_H
#define I2C_HAL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "stm32f1xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    I2C_HandleTypeDef  hi2c;      /**< HAL handle (persistent) */
    bool               _init;
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
