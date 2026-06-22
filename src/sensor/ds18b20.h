/**
 * @file    ds18b20.h
 * @brief   DS18B20 温度传感器 — implements Sensor interface (vtable OOP)
 *
 * ── 类继承 ──────────────────────────────────────────────
 *   Sensor (abstract)
 *     └── DS18B20 (concrete)
 *   DS18B20.base.vtable → ds18b20_vtable (read / name / is_present / init)
 */

#ifndef DS18B20_H
#define DS18B20_H

#include "sensor.h"
#include "onewire.h"

/* ── DS18B20 命令 ─────────────────────────────────────── */
#define DS18B20_CMD_CONVERT_T       0x44
#define DS18B20_CMD_READ_SCRATCH    0xBE
#define DS18B20_CMD_SKIP_ROM        0xCC
#define DS18B20_CMD_READ_ROM        0x33

typedef struct {
    Sensor      base;        /**< "继承" Sensor 接口 */
    OneWireBus *bus;         /**< 1-Wire 总线 */
    uint8_t     rom[8];      /**< 64-bit ROM 码 */
} DS18B20;

/**
 * @brief 构造函数 — 创建 DS18B20 并绑定总线
 * @param self 未初始化的 DS18B20 对象 (调用者分配)
 * @param bus  已初始化的 OneWire 总线
 * @return Sensor* 接口指针 (就是 &self->base)
 */
Sensor *ds18b20_create(DS18B20 *self, OneWireBus *bus);

/* ── 低层函数 (仍可用, 但推荐通过 sensor.h 统一调用) ──── */
void ds18b20_start_conversion(DS18B20 *self);
void ds18b20_wait_conversion(DS18B20 *self);
bool ds18b20_read_scratchpad(DS18B20 *self, uint8_t buf[9]);
float ds18b20_parse_temp(const uint8_t scratchpad[9]);

#endif
