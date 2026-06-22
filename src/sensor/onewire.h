/**
 * @file    onewire.h
 * @brief   1-Wire 总线驱动 — 基于 GPIO 位操作
 *
 * Dallas/Maxim 1-Wire 协议实现。使用开漏 + 上拉模式,
 * 通过延时实现精确时序。
 *
 * 时序要求 (标准模式):
 *   Reset:    拉低 480µs → 释放 → 采样存在脉冲
 *   Write 1:  拉低 1-15µs → 释放 → 保持 60µs
 *   Write 0:  拉低 60-120µs → 释放
 *   Read:     拉低 1-15µs → 释放 → 在 15µs 内采样
 *
 * 注意: 操作期间需关中断以防止时序被破坏。
 *       每条总线最多挂 127 个设备。
 *
 * PA1 = DS18B20 DQ (需要 4.7kΩ 上拉电阻到 3.3V)
 */

#ifndef ONEWIRE_H
#define ONEWIRE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    void     *port;      /**< GPIO 端口 */
    uint16_t  pin;       /**< GPIO 引脚 */
    uint8_t   _init;     /**< 私有: 初始化标记 */
} OneWireBus;

/**
 * @brief 初始化 1-Wire 总线
 * @param bus  总线对象
 * @param port GPIO 端口 (GPIOA, GPIOB, GPIOC)
 * @param pin  引脚位掩码 (GPIO_PIN_1 等)
 */
void ow_init(OneWireBus *bus, void *port, uint16_t pin);

/**
 * @brief 复位总线并检测设备存在
 * @return true 有设备应答存在脉冲
 */
bool ow_reset(OneWireBus *bus);

/** @brief 写一个 bit */
void ow_write_bit(OneWireBus *bus, uint8_t bit);

/** @brief 读一个 bit */
uint8_t ow_read_bit(OneWireBus *bus);

/** @brief 写一个字节 (LSB first) */
void ow_write_byte(OneWireBus *bus, uint8_t byte);

/** @brief 读一个字节 (LSB first) */
uint8_t ow_read_byte(OneWireBus *bus);

/**
 * @brief 搜索总线上的 ROM 码
 *
 * 使用二叉树搜索算法, 发现总线上的所有设备地址。
 *
 * @param bus   总线对象
 * @param rom   输出: ROM 码数组 (8 bytes × max_devices)
 * @param max   最大搜索设备数
 * @return 实际找到的设备数
 */
int ow_search_rom(OneWireBus *bus, uint8_t rom[][8], int max);

/**
 * @brief 计算 8 字节数据的 Dallas CRC8
 * @param data 数据指针
 * @param len  长度
 * @return CRC8 校验值
 */
uint8_t ow_crc8(const uint8_t *data, int len);

#ifdef __cplusplus
}
#endif

#endif /* ONEWIRE_H */
