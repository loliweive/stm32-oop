#include "i2c.h"
#include "stm32f103xb.h"

void I2cPort_ctor(I2cPort *self, void *i2c, uint32_t speed_hz, uint32_t pclk_hz)
{
    self->i2c      = i2c;
    self->speed_hz = speed_hz;
    self->pclk_hz  = pclk_hz;
    self->_init    = false;
}

void i2c_init(I2cPort *self)
{
    I2C_Type *i = (I2C_Type *)self->i2c;

    /* Reset I2C */
    i->CR1 |= 0x8000; /* SWRST */
    i->CR1 &= ~0x8000;

    /* Configure clock: CCR = pclk / (2 * speed) in standard mode */
    uint16_t ccr = (uint16_t)(self->pclk_hz / (2 * self->speed_hz));
    if (ccr < 4) ccr = 4;
    i->CR2 = self->pclk_hz / 1000000;  /* FREQ = pclk in MHz */
    i->CCR = ccr;
    i->TRISE = (self->pclk_hz / 1000000) + 1;  /* Max rise time */

    i->CR1 |= 0x01;  /* PE: peripheral enable */
    self->_init = true;
}

static void i2c_start(I2C_Type *i)
{
    i->CR1 |= 0x100; /* START */
    while (!(i->SR1 & 0x01)) {} /* Wait SB */
}

static void i2c_stop(I2C_Type *i)
{
    i->CR1 |= 0x200; /* STOP */
}

static void i2c_addr(I2C_Type *i, uint8_t addr, uint8_t dir)
{
    i->DR = (uint8_t)((addr << 1) | dir);
    while (!(i->SR1 & 0x02)) {} /* Wait ADDR */
    (void)i->SR2; /* Clear ADDR */
}

static void i2c_write_byte(I2C_Type *i, uint8_t byte)
{
    while (!(i->SR1 & 0x80)) {} /* Wait TxE */
    i->DR = byte;
    while (!(i->SR1 & 0x80)) {} /* Wait TxE */
}

bool i2c_write(I2cPort *self, uint8_t addr, const uint8_t *data, size_t len)
{
    I2C_Type *i = (I2C_Type *)self->i2c;
    i2c_start(i);
    i2c_addr(i, addr, 0);  /* Write direction */
    for (size_t n = 0; n < len; n++) {
        i2c_write_byte(i, data[n]);
    }
    i2c_stop(i);
    return true;
}

bool i2c_read(I2cPort *self, uint8_t addr, uint8_t *data, size_t len)
{
    I2C_Type *i = (I2C_Type *)self->i2c;
    i2c_start(i);
    i2c_addr(i, addr, 1);  /* Read direction */

    for (size_t n = 0; n < len; n++) {
        if (n == len - 1) {
            i->CR1 &= ~0x400; /* ACK off for last byte */
        }
        while (!(i->SR1 & 0x40)) {} /* Wait RxNE */
        data[n] = (uint8_t)i->DR;
    }
    i2c_stop(i);
    i->CR1 |= 0x400;  /* ACK back on */
    return true;
}

bool i2c_write_reg(I2cPort *self, uint8_t addr, uint8_t reg, uint8_t val)
{
    uint8_t buf[] = {reg, val};
    return i2c_write(self, addr, buf, 2);
}

bool i2c_read_reg(I2cPort *self, uint8_t addr, uint8_t reg, uint8_t *val)
{
    i2c_write(self, addr, &reg, 1);
    return i2c_read(self, addr, val, 1);
}
