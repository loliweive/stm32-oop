/**
 * @file    dht11.h
 * @brief   DHT11 温湿度传感器 — implements Sensor interface (vtable OOP)
 *
 * ── 类继承 ──────────────────────────────────────────────
 *   Sensor (abstract)
 *     └── DHT11 (concrete)
 *   DHT11.base.vtable → dht11_vtable (read / name / is_present / init)
 */

#ifndef DHT11_H
#define DHT11_H

#include "sensor.h"

typedef struct {
    Sensor   base;         /**< "继承" Sensor 接口 */
    void    *port;         /**< GPIO 端口 */
    uint16_t pin;          /**< GPIO 引脚 */
    uint8_t  humidity;     /**< 最近湿度 (%RH) */
    uint8_t  temp_c;       /**< 最近温度 (°C) */
} DHT11;

/**
 * @brief 构造函数 — 创建 DHT11 并绑定引脚
 * @param self 未初始化的 DHT11 对象 (调用者分配)
 * @param port GPIO 端口 (GPIOA)
 * @param pin  引脚位掩码 (GPIO_PIN_1)
 * @return Sensor* 接口指针
 */
Sensor *dht11_create(DHT11 *self, void *port, uint16_t pin);

/** @brief 低层读取 (阻塞 ~25ms) */
bool dht11_raw_read(DHT11 *self);

#endif
