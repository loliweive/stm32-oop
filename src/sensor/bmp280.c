/**
 * @file    bmp280.c
 * @brief   BMP280 温度+气压传感器实现 — Sensor OOP vtable
 *
 * 使用 I2cPort HAL 进行 I2C 通信 (I2C1, 共用 OLED 总线)。
 * 支持 forced mode: 每次读取触发一次测量, 完成后读取结果。
 */

#include "bmp280.h"
#include "i2c.h"
#include <math.h>

/* ── I2C 辅助 ────────────────────────────────────────── */

static bool _write_reg(BMP280 *bmp, uint8_t reg, uint8_t val) {
    I2cPort p; I2cPort_ctor(&p, bmp->i2c, 100000, 36000000);
    return i2c_write_reg(&p, bmp->addr, reg, val);
}

static bool _read_reg(BMP280 *bmp, uint8_t reg, uint8_t *val) {
    I2cPort p; I2cPort_ctor(&p, bmp->i2c, 100000, 36000000);
    return i2c_read_reg(&p, bmp->addr, reg, val);
}

/* BMP280 multi-byte read: write register, restart, read N bytes */
#include "stm32f103xb.h"
static bool _read_multi(BMP280 *bmp, uint8_t reg, uint8_t *buf, size_t len) {
    I2C_Type *i = (I2C_Type *)bmp->i2c;
    uint32_t to;

    /* START + send device addr (write) + register */
    to = 100000; while ((i->SR2 & I2C_SR2_BUSY) && --to) {}
    i->CR1 |= I2C_CR1_START;
    to = 100000; while (!(i->SR1 & I2C_SR1_SB) && --to) {}
    if (!to) { i->CR1 |= I2C_CR1_STOP; return false; }
    i->DR = (uint8_t)(bmp->addr << 1);  /* write mode */
    to = 100000; while (!(i->SR1 & I2C_SR1_ADDR) && --to) {}
    (void)i->SR2;
    to = 100000; while (!(i->SR1 & I2C_SR1_TXE) && --to) {}
    i->DR = reg;
    to = 100000; while (!(i->SR1 & I2C_SR1_BTF) && --to) {}

    /* RESTART + send device addr (read) */
    i->CR1 |= I2C_CR1_START;
    to = 100000; while (!(i->SR1 & I2C_SR1_SB) && --to) {}
    if (!to) { i->CR1 |= I2C_CR1_STOP; return false; }
    i->DR = (uint8_t)((bmp->addr << 1) | 1);  /* read mode */
    to = 100000; while (!(i->SR1 & I2C_SR1_ADDR) && --to) {}
    (void)i->SR2;

    /* Read data bytes */
    for (size_t n = 0; n < len; n++) {
        /* Set ACK for all but last byte */
        if (n == len - 1) i->CR1 &= ~I2C_CR1_ACK;
        else              i->CR1 |= I2C_CR1_ACK;
        to = 100000; while (!(i->SR1 & I2C_SR1_RXNE) && --to) {}
        buf[n] = (uint8_t)i->DR;
    }
    i->CR1 |= I2C_CR1_STOP;
    return true;
}

/* ── 校准算法 (BMP280 datasheet §4.2.3) ────────────────── */

static int32_t _compensate_T(BMP280 *bmp, int32_t adc_T) {
    int32_t var1, var2;
    var1 = ((((adc_T >> 3) - ((int32_t)bmp->cal.dig_T1 << 1))) *
            ((int32_t)bmp->cal.dig_T2)) >> 11;
    var2 = (((((adc_T >> 4) - ((int32_t)bmp->cal.dig_T1)) *
              ((adc_T >> 4) - ((int32_t)bmp->cal.dig_T1))) >> 12) *
            ((int32_t)bmp->cal.dig_T3)) >> 14;
    bmp->t_fine = var1 + var2;
    return (bmp->t_fine * 5 + 128) >> 8;  /* °C × 100 */
}

static uint32_t _compensate_P(BMP280 *bmp, int32_t adc_P) {
    int64_t var1, var2, p;
    var1 = ((int64_t)bmp->t_fine) - 128000;
    var2 = var1 * var1 * (int64_t)bmp->cal.dig_P6;
    var2 = var2 + ((var1 * (int64_t)bmp->cal.dig_P5) << 17);
    var2 = var2 + (((int64_t)bmp->cal.dig_P4) << 35);
    var1 = ((var1 * var1 * (int64_t)bmp->cal.dig_P3) >> 8) +
           ((var1 * (int64_t)bmp->cal.dig_P2) << 12);
    var1 = (((((int64_t)1) << 47) + var1)) * ((int64_t)bmp->cal.dig_P1) >> 33;
    if (var1 == 0) return 0;
    p = 1048576 - adc_P;
    p = (((p << 31) - var2) * 3125) / var1;
    var1 = (((int64_t)bmp->cal.dig_P9) * (p >> 13) * (p >> 13)) >> 25;
    var2 = (((int64_t)bmp->cal.dig_P8) * p) >> 19;
    p = ((p + var1 + var2) >> 8) + (((int64_t)bmp->cal.dig_P7) << 4);
    return (uint32_t)p;
}

/* ── 加载校准数据 ─────────────────────────────────────── */

static bool _load_calib(BMP280 *bmp) {
    uint8_t buf[24];
    if (!_read_multi(bmp, 0x88, buf, 24)) return false;

    bmp->cal.dig_T1 = (uint16_t)(buf[0] | (buf[1] << 8));
    bmp->cal.dig_T2 = (int16_t)(buf[2] | (buf[3] << 8));
    bmp->cal.dig_T3 = (int16_t)(buf[4] | (buf[5] << 8));
    bmp->cal.dig_P1 = (uint16_t)(buf[6] | (buf[7] << 8));
    bmp->cal.dig_P2 = (int16_t)(buf[8] | (buf[9] << 8));
    bmp->cal.dig_P3 = (int16_t)(buf[10] | (buf[11] << 8));
    bmp->cal.dig_P4 = (int16_t)(buf[12] | (buf[13] << 8));
    bmp->cal.dig_P5 = (int16_t)(buf[14] | (buf[15] << 8));
    bmp->cal.dig_P6 = (int16_t)(buf[16] | (buf[17] << 8));
    bmp->cal.dig_P7 = (int16_t)(buf[18] | (buf[19] << 8));
    bmp->cal.dig_P8 = (int16_t)(buf[20] | (buf[21] << 8));
    bmp->cal.dig_P9 = (int16_t)(buf[22] | (buf[23] << 8));
    return true;
}

/* ── vtable 实现 ────────────────────────────────────────── */

static bool _read(Sensor *base, float *temp_c, uint8_t *humidity) {
    BMP280 *self = (BMP280 *)base;

    /* Forced mode: set osrs_t=1 osrs_p=1 mode=forced */
    _write_reg(self, 0xF4, 0x25);  /* 0x25 = 0010 0101 */

    /* Wait for measurement (max 9.3ms) */
    for (volatile int i = 0; i < 720000; i++) __asm__("nop");  /* ~10ms @72MHz */

    /* Read data (6 bytes: press_msb, press_lsb, press_xlsb, temp_msb, temp_lsb, temp_xlsb) */
    uint8_t data[6];
    if (!_read_multi(self, 0xF7, data, 6)) {
        if (temp_c) *temp_c = -273.0f;
        if (humidity) *humidity = 255;
        return false;
    }

    int32_t adc_P __attribute__((unused)) = ((int32_t)data[0] << 12) | ((int32_t)data[1] << 4) | (data[2] >> 4);
    int32_t adc_T = ((int32_t)data[3] << 12) | ((int32_t)data[4] << 4) | (data[5] >> 4);

    int32_t t_raw = _compensate_T(self, adc_T);
    float t = (float)t_raw / 100.0f;

    if (temp_c) *temp_c = t;
    if (humidity) *humidity = 255;  /* BMP280 不支持湿度 */
    return true;
}

static const char *_name(Sensor *base)     { (void)base; return "BMP280"; }

static bool _is_present(Sensor *base) {
    BMP280 *self = (BMP280 *)base;
    uint8_t id;
    if (!_read_reg(self, 0xD0, &id)) return false;
    return (id == 0x58);
}

static bool _init(Sensor *base) {
    BMP280 *self = (BMP280 *)base;

    /* 验证芯片 ID */
    if (!_is_present(base)) return false;

    /* 加载校准数据 */
    if (!_load_calib(self)) return false;

    /* 配置: t_standby=0.5ms, filter=off, SPI=off */
    _write_reg(self, 0xF5, 0x00);

    return true;
}

static const SensorVtable bmp280_vtable = {
    .read       = _read,
    .name       = _name,
    .is_present = _is_present,
    .init       = _init,
};

/* ── 构造函数 ──────────────────────────────────────────── */

Sensor *bmp280_create(BMP280 *self, void *i2c, uint8_t addr) {
    self->base.vtable = &bmp280_vtable;
    self->i2c  = i2c;
    self->addr = addr;
    self->t_fine = 0;
    if (!_init(&self->base)) {
        /* 初始化失败 — 标记不存在 */
        return (Sensor *)self;  /* 仍返回指针, is_present() 返回 false */
    }
    return &self->base;
}

/* ── BMP280 专有: 读取气压 ──────────────────────────────── */

float bmp280_get_pressure(Sensor *base) {
    BMP280 *self = (BMP280 *)base;

    /* 触发测量并读取 */
    _write_reg(self, 0xF4, 0x25);  /* forced mode */
    for (volatile int i = 0; i < 720000; i++) __asm__("nop");

    uint8_t data[6];
    if (!_read_multi(self, 0xF7, data, 6)) return -1.0f;

    int32_t adc_P = ((int32_t)data[0] << 12) | ((int32_t)data[1] << 4) | (data[2] >> 4);
    int32_t adc_T = ((int32_t)data[3] << 12) | ((int32_t)data[4] << 4) | (data[5] >> 4);

    /* 先计算温度补偿参数 */
    _compensate_T(self, adc_T);

    /* 计算气压 */
    uint32_t p_raw = _compensate_P(self, adc_P);
    return (float)p_raw / 25600.0f;  /* Pa → hPa */
}
