/**
 * @file    dht11.h
 * @brief   DHT11 数字温湿度传感器驱动
 *
 * 协议: 单总线 (非 OneWire — DHT11 自有协议)
 * 精度: ±2°C 温度, ±5%RH 湿度
 * 范围: 0-50°C, 20-90%RH
 * 采样: 1Hz (最大)
 *
 * 接线:
 *   VDD → 3.3V
 *   GND → GND
 *   DAT → PA1 (需要 4.7kΩ 上拉到 3.3V)
 *
 * ── 协议时序 ─────────────────────────────────────────────
 *   主机发送起始信号:
 *     拉低 18ms → 拉高 20-40µs → 释放
 *   DHT11 应答:
 *     拉低 80µs → 拉高 80µs
 *   数据 (40 bits):
 *     Bit 0: 50µs 低 + 26-28µs 高
 *     Bit 1: 50µs 低 + 70µs 高
 *   数据格式:
 *     [湿度整数][湿度小数][温度整数][温度小数][校验和]
 *     校验和 = 前 4 字节之和的低 8 位
 */

#ifndef DHT11_H
#define DHT11_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    void     *port;       /**< GPIO 端口 */
    uint16_t  pin;        /**< GPIO 引脚 */
    uint8_t   humidity;   /**< 最近一次湿度 (%RH, 整数) */
    uint8_t   temp_c;     /**< 最近一次温度 (°C) */
    bool      present;    /**< 设备是否在线 */
} DHT11;

/**
 * @brief 初始化 DHT11
 * @param dht  传感器对象
 * @param port GPIO 端口 (GPIOA)
 * @param pin  引脚位掩码 (GPIO_PIN_1)
 */
void dht11_init(DHT11 *dht, void *port, uint16_t pin);

/**
 * @brief 读取一次温湿度 (阻塞, ~25ms)
 *
 * 包含起始信号 + 等待应答 + 读取 40 bits。
 * DHT11 需要上电后等待 1 秒才能首次读取。
 *
 * @param dht 传感器对象
 * @return true 读取成功 (校验和通过)
 */
bool dht11_read(DHT11 *dht);

/** @brief 获取温度 (°C) */
uint8_t dht11_get_temp(const DHT11 *dht);

/** @brief 获取湿度 (%RH) */
uint8_t dht11_get_humidity(const DHT11 *dht);

/** @brief 设备是否在线 */
bool dht11_is_present(const DHT11 *dht);

#ifdef __cplusplus
}
#endif

#endif /* DHT11_H */
