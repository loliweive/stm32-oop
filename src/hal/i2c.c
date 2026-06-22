/**
 * @file    i2c.c
 * @brief   I2C 主机驱动 — 使用 CMSIS 位定义，带超时保护
 */
#include "i2c.h"
#include "stm32f103xb.h"

/* I2C 超时计数器 — 约 10ms @ 72MHz */
#define I2C_TIMEOUT 100000UL

/** @brief 带超时的标志等待，返回 true=就绪，false=超时 */
static bool i2c_wait_sr1(I2C_Type *i, uint32_t flag)
{
    uint32_t timeout = I2C_TIMEOUT;
    while (!(i->SR1 & flag) && --timeout) {}
    return timeout > 0;
}

/** @brief 检查错误标志 (AF, BERR, ARLO) */
static bool i2c_error(I2C_Type *i)
{
    return (i->SR1 & (I2C_SR1_AF | I2C_SR1_BERR | I2C_SR1_ARLO)) != 0;
}

void I2cPort_ctor(I2cPort *self, void *i2c, uint32_t speed_hz, uint32_t pclk_hz)
{
    self->i2c      = i2c;
    self->speed_hz = speed_hz ? speed_hz : 100000;
    self->pclk_hz  = pclk_hz  ? pclk_hz  : 36000000;
    self->_init    = false;
}

void i2c_init(I2cPort *self)
{
    I2C_Type *i = (I2C_Type *)self->i2c;

    /* SWRST: 软件复位 I2C 外设 */
    i->CR1 |= I2C_CR1_SWRST;
    i->CR1 &= ~I2C_CR1_SWRST;

    /* FREQ = pclk MHz (最大 36) */
    i->CR2 = (self->pclk_hz / 1000000) < 36 ? (self->pclk_hz / 1000000) : 36;

    /* CCR = pclk / (2 * speed) — 标准模式 */
    uint16_t ccr = (uint16_t)(self->pclk_hz / (2 * self->speed_hz));
    if (ccr < 4) ccr = 4;
    i->CCR = ccr;

    /* TRISE: 最大上升时间 (标准模式: 1000ns) */
    i->TRISE = (self->pclk_hz / 1000000) + 1;

    /* PE: 使能 I2C */
    i->CR1 |= I2C_CR1_PE;
    self->_init = true;
}

/** @brief 发送 START 条件并等待 SB 标志 */
static bool i2c_start(I2C_Type *i)
{
    i->CR1 |= I2C_CR1_START;
    if (!i2c_wait_sr1(i, I2C_SR1_SB)) return false;
    return true;
}

/** @brief 发送 STOP 条件 */
static void i2c_stop(I2C_Type *i)
{
    i->CR1 |= I2C_CR1_STOP;
}

/** @brief 发送地址并等待 ADDR 标志 */
static bool i2c_send_addr(I2C_Type *i, uint8_t addr, uint8_t dir)
{
    i->DR = (uint8_t)((addr << 1) | dir);
    if (!i2c_wait_sr1(i, I2C_SR1_ADDR)) return false;
    (void)i->SR2; /* 读 SR2 清除 ADDR 标志 */
    return true;
}

/** @brief 发送一个字节 */
static bool i2c_send_byte(I2C_Type *i, uint8_t byte)
{
    if (!i2c_wait_sr1(i, I2C_SR1_TXE)) return false;
    i->DR = byte;
    if (!i2c_wait_sr1(i, I2C_SR1_TXE)) return false;
    /* 检查 AF (从机未应答) */
    if (i->SR1 & I2C_SR1_AF) {
        i->SR1 &= ~I2C_SR1_AF;
        return false;
    }
    return true;
}

bool i2c_write(I2cPort *self, uint8_t addr, const uint8_t *data, size_t len)
{
    if (addr > 0x7F || !data || len == 0) return false;

    I2C_Type *i = (I2C_Type *)self->i2c;
    if (!i2c_start(i)) { i2c_stop(i); return false; }
    if (!i2c_send_addr(i, addr, 0)) { i2c_stop(i); return false; }
    if (i2c_error(i)) { i2c_stop(i); return false; }

    for (size_t n = 0; n < len; n++) {
        if (!i2c_send_byte(i, data[n])) { i2c_stop(i); return false; }
    }
    i2c_stop(i);
    return true;
}

bool i2c_read(I2cPort *self, uint8_t addr, uint8_t *data, size_t len)
{
    if (addr > 0x7F || !data || len == 0) return false;

    I2C_Type *i = (I2C_Type *)self->i2c;
    if (!i2c_start(i)) { i2c_stop(i); return false; }
    if (!i2c_send_addr(i, addr, 1)) { i2c_stop(i); return false; }

    for (size_t n = 0; n < len; n++) {
        /* 最后一个字节前关闭 ACK → 发送 NACK */
        if (n == len - 1) {
            i->CR1 &= ~I2C_CR1_ACK;
        }
        if (!i2c_wait_sr1(i, I2C_SR1_RXNE)) { i2c_stop(i); return false; }
        data[n] = (uint8_t)i->DR;
    }
    i2c_stop(i);
    i->CR1 |= I2C_CR1_ACK;  /* 恢复 ACK */
    return true;
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
