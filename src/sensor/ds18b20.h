/**
 * @file    ds18b20.h
 * @brief   DS18B20 数字温度传感器驱动 (Dallas/Maxim 1-Wire)
 *
 * 精度: 12-bit (默认), 0.0625°C / LSB
 * 范围: -55°C ~ +125°C
 * 转换时间: 750ms (max @ 12-bit)
 *
 * 依赖: onewire.h (1-Wire 总线驱动)
 *
 * 接线:
 *   VDD  → 3.3V
 *   GND  → GND
 *   DQ   → PA1 (MCU GPIO, 开漏 + 4.7kΩ 上拉到 3.3V)
 *
 * ── 使用示例 ─────────────────────────────────────────────
 *   OneWireBus bus;
 *   ow_init(&bus, GPIOA, GPIO_PIN_1);
 *
 *   DS18B20 sensor;
 *   ds18b20_init(&sensor, &bus);
 *
 *   float temp;
 *   if (ds18b20_read_temp(&sensor, &temp)) {
 *       printf("Temperature: %.2f°C\n", temp);
 *   }
 */

#ifndef DS18B20_H
#define DS18B20_H

#include <stdint.h>
#include <stdbool.h>
#include "onewire.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── DS18B20 ROM 命令 ─────────────────────────────────────────── */
#define DS18B20_CMD_SEARCH_ROM      0xF0
#define DS18B20_CMD_READ_ROM        0x33
#define DS18B20_CMD_MATCH_ROM       0x55
#define DS18B20_CMD_SKIP_ROM        0xCC
#define DS18B20_CMD_ALARM_SEARCH    0xEC

/* ── DS18B20 功能命令 ─────────────────────────────────────────── */
#define DS18B20_CMD_CONVERT_T       0x44  /* 启动温度转换 */
#define DS18B20_CMD_WRITE_SCRATCH   0x4E  /* 写暂存器 */
#define DS18B20_CMD_READ_SCRATCH    0xBE  /* 读暂存器 (9 bytes) */
#define DS18B20_CMD_COPY_SCRATCH    0x48  /* 复制暂存器到 EEPROM */
#define DS18B20_CMD_RECALL_EE       0xB8  /* 从 EEPROM 恢复 */

/* ── 分辨率 ────────────────────────────────────────────────────── */
typedef enum {
    DS18B20_RES_9BIT  = 0x1F,  /* 93.75ms 转换 */
    DS18B20_RES_10BIT = 0x3F,  /* 187.5ms */
    DS18B20_RES_11BIT = 0x5F,  /* 375ms   */
    DS18B20_RES_12BIT = 0x7F,  /* 750ms   (默认) */
} DS18B20Resolution;

/* ── 传感器结构 ────────────────────────────────────────────────── */
typedef struct {
    OneWireBus *bus;         /**< 1-Wire 总线 */
    uint8_t    rom[8];       /**< 64-bit ROM 码 */
    bool       present;      /**< 设备是否在线 */
} DS18B20;

/**
 * @brief 初始化传感器 — 检测设备并读取 ROM 码
 * @param sensor 传感器对象
 * @param bus    已初始化的 1-Wire 总线
 * @return true 设备在线
 */
bool ds18b20_init(DS18B20 *sensor, OneWireBus *bus);

/**
 * @brief 启动温度转换 (非阻塞)
 *
 * 发送 CONVERT_T 命令。转换在后台进行 (12-bit 需 750ms)。
 * 转换期间总线被设备拉低 (可通过读时隙检测完成)。
 */
void ds18b20_start_conversion(DS18B20 *sensor);

/**
 * @brief 等待温度转换完成 (阻塞)
 *
 * 轮询总线直到设备释放 (转换完成)。
 * 最大等待: ~800ms
 */
void ds18b20_wait_conversion(DS18B20 *sensor);

/**
 * @brief 读取温度值 (阻塞 — 包含转换等待)
 *
 * 完整流程: 启动转换 → 等待 → 读暂存器 → 解析温度
 *
 * @param sensor  传感器对象
 * @param temp_c  输出: 摄氏温度
 * @return true 读取成功 (CRC 校验通过)
 */
bool ds18b20_read_temp(DS18B20 *sensor, float *temp_c);

/**
 * @brief 读取原始暂存器数据 (9 bytes)
 *
 * 格式:
 *   [0]   温度 LSB
 *   [1]   温度 MSB
 *   [2]   TH 高温报警
 *   [3]   TL 低温报警
 *   [4]   配置寄存器 (分辨率)
 *   [5-7] 保留 (0xFF)
 *   [8]   CRC8
 *
 * @return true CRC 校验通过
 */
bool ds18b20_read_scratchpad(DS18B20 *sensor, uint8_t buf[9]);

/**
 * @brief 解析温度值 (从暂存器原始数据)
 * @param scratchpad 暂存器 9 bytes
 * @return 摄氏温度
 */
float ds18b20_parse_temp(const uint8_t scratchpad[9]);

#ifdef __cplusplus
}
#endif

#endif /* DS18B20_H */
