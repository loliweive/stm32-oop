/**
 * @file    bmp280.h
 * @brief   BMP280 温度+气压传感器 — Sensor OOP vtable 实现
 *
 * I2C 地址: 0x76 (SDO=GND) 或 0x77 (SDO=VCC)
 * 接线: SCL→PB6, SDA→PB7 (共用 I2C1 总线, 与 OLED 并存)
 *
 * ── 使用方法 ────────────────────────────────────────────
 *   BMP280 bmp;
 *   Sensor *s = bmp280_create(&bmp, I2C1, 0x76);
 *   float temp; uint8_t dummy;
 *   sensor_read(s, &temp, &dummy);
 *   float pressure = bmp280_get_pressure(s);  // hPa
 */

#ifndef BMP280_H
#define BMP280_H

#include "sensor.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* BMP280 校准数据结构 (26 bytes from registers 0x88-0xA1) */
typedef struct {
    uint16_t dig_T1;
    int16_t  dig_T2;
    int16_t  dig_T3;
    uint16_t dig_P1;
    int16_t  dig_P2;
    int16_t  dig_P3;
    int16_t  dig_P4;
    int16_t  dig_P5;
    int16_t  dig_P6;
    int16_t  dig_P7;
    int16_t  dig_P8;
    int16_t  dig_P9;
} Bmp280Calib;

typedef struct {
    Sensor       base;          /**< 基类 */
    void        *i2c;           /**< I2C_Type* */
    uint8_t      addr;          /**< I2C 地址 */
    Bmp280Calib  cal;           /**< 校准数据 */
    int32_t      t_fine;        /**< 温度补偿参数 (用于气压计算) */
} BMP280;

/**
 * @brief 创建 BMP280 传感器
 * @param self  未初始化的 BMP280 实例
 * @param i2c   I2C 外设 (I2C1/I2C2)
 * @param addr  I2C 地址 (通常 0x76)
 * @return Sensor 基类指针 (用于多态)
 */
Sensor *bmp280_create(BMP280 *self, void *i2c, uint8_t addr);

/**
 * @brief 获取气压 (BMP280 专有, 不在 Sensor 接口中)
 * @param base 必须是 BMP280 的 Sensor* 指针
 * @return 气压 (hPa), -1 表示无效
 */
float bmp280_get_pressure(Sensor *base);

#ifdef __cplusplus
}
#endif

#endif /* BMP280_H */
