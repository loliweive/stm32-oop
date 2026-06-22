/**
 * @file    light_sensor.h
 * @brief   光敏电阻模块 — implements Sensor interface (vtable OOP)
 *
 * M7 光敏电阻模块:
 *   AO/ADC → PA3  (ADC1_IN3, 模拟光照强度 0-3.3V)
 *   DO/INT → PB11 (数字阈值输出, 0=亮/1=暗, 电位器可调)
 *   VCC    → 3.3V
 *   GND    → GND
 *
 * ── 类继承 ──────────────────────────────────────────────
 *   Sensor (abstract)
 *     └── LightSensor (concrete)
 *
 *   sensor_read() 返回:
 *     temp_c   = 照度百分比 0.0-100.0 (模拟量, 0%=最暗)
 *     humidity = 数字阈值状态 (0=亮/1=暗)
 */

#ifndef LIGHT_SENSOR_H
#define LIGHT_SENSOR_H

#include "sensor.h"

typedef struct {
    Sensor   base;         /**< "继承" Sensor 接口 */
    void    *adc;          /**< ADC 外设 (ADC1) */
    uint8_t  adc_channel;  /**< ADC 通道 (PA3 = channel 3) */
    void    *do_port;      /**< 数字输出 GPIO 端口 (GPIOB) */
    uint16_t do_pin;       /**< 数字输出引脚 (PB11 = GPIO_PIN_11) */
    float    lux_pct;      /**< 最近光照百分比 */
    uint8_t  digital;      /**< 最近数字状态 (0=亮/1=暗) */
} LightSensor;

/**
 * @brief 构造函数 — 创建光敏传感器
 * @param self        未初始化的对象
 * @param adc         ADC 外设 (ADC1)
 * @param adc_channel ADC 通道 (PA3 = 3)
 * @param do_port     数字 GPIO 端口 (GPIOB)
 * @param do_pin      数字 GPIO 引脚 (GPIO_PIN_11)
 * @return Sensor* 接口指针
 */
Sensor *light_sensor_create(LightSensor *self, void *adc, uint8_t adc_channel,
                            void *do_port, uint16_t do_pin);

#endif
