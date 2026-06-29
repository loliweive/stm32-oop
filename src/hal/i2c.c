/**
 * @file    i2c.c
 * @brief   I2C master — HAL API internally
 */
#include "i2c.h"

void I2cPort_ctor(I2cPort *self, void *i2c, uint32_t speed_hz, uint32_t pclk_hz)
{
    self->hi2c.Instance = (I2C_TypeDef *)i2c;
    self->hi2c.Init.ClockSpeed      = speed_hz ? speed_hz : 100000;
    self->hi2c.Init.DutyCycle       = I2C_DUTYCYCLE_2;
    self->hi2c.Init.OwnAddress1     = 0;
    self->hi2c.Init.AddressingMode  = I2C_ADDRESSINGMODE_7BIT;
    self->hi2c.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    self->hi2c.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    self->hi2c.Init.NoStretchMode   = I2C_NOSTRETCH_DISABLE;
    (void)pclk_hz; /* HAL computes timing from ClockSpeed */
    self->_init = false;
}

void i2c_init(I2cPort *self)
{
    HAL_I2C_Init(&self->hi2c);
    self->_init = true;
}

bool i2c_write(I2cPort *self, uint8_t addr, const uint8_t *data, size_t len)
{
    if (addr > 0x7F || !data || len == 0) return false;
    return HAL_I2C_Master_Transmit(&self->hi2c, (uint16_t)(addr << 1),
                                   (uint8_t *)data, (uint16_t)len, 100) == HAL_OK;
}

bool i2c_read(I2cPort *self, uint8_t addr, uint8_t *data, size_t len)
{
    if (addr > 0x7F || !data || len == 0) return false;
    return HAL_I2C_Master_Receive(&self->hi2c, (uint16_t)(addr << 1),
                                  data, (uint16_t)len, 100) == HAL_OK;
}

bool i2c_write_reg(I2cPort *self, uint8_t addr, uint8_t reg, uint8_t val)
{
    uint8_t buf[] = {reg, val};
    return i2c_write(self, addr, buf, 2);
}

bool i2c_read_reg(I2cPort *self, uint8_t addr, uint8_t reg, uint8_t *val)
{
    if (!i2c_write(self, addr, &reg, 1)) return false;
    return i2c_read(self, addr, val, 1);
}
