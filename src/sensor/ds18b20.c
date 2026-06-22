/**
 * @file    ds18b20.c
 * @brief   DS18B20 实现 — Sensor vtable + OneWire 操作
 */
#include "ds18b20.h"

/* ── vtable 实现 ────────────────────────────────────────── */

static bool _read(Sensor *base, float *temp_c, uint8_t *humidity)
{
    DS18B20 *self = (DS18B20 *)base;
    uint8_t sp[9];

    ds18b20_start_conversion(self);
    ds18b20_wait_conversion(self);

    if (!ds18b20_read_scratchpad(self, sp)) {
        if (temp_c)   *temp_c   = -273.0f;
        if (humidity) *humidity = 255;
        return false;
    }
    if (temp_c)   *temp_c   = ds18b20_parse_temp(sp);
    if (humidity) *humidity = 255;  /* DS18B20 不支持湿度 */
    return true;
}

static const char *_name(Sensor *base)       { (void)base; return "DS18B20"; }
static bool _is_present(Sensor *base)        { return ((DS18B20 *)base)->bus != NULL; }
static bool _init(Sensor *base)              { return true; } /* 初始化在 create 中完成 */

static const SensorVtable ds18b20_vtable = {
    .read        = _read,
    .name        = _name,
    .is_present  = _is_present,
    .init        = _init,
};

/* ── 构造函数 ──────────────────────────────────────────── */

Sensor *ds18b20_create(DS18B20 *self, OneWireBus *bus)
{
    self->base.vtable = &ds18b20_vtable;
    self->bus         = bus;

    /* 读 ROM 验证设备存在 */
    if (ow_reset(bus)) {
        ow_write_byte(bus, DS18B20_CMD_READ_ROM);
        for (int i = 0; i < 8; i++) self->rom[i] = ow_read_byte(bus);
        if (ow_crc8(self->rom, 8) != 0) {
            self->bus = NULL; /* 标记为不存在 */
        }
    } else {
        self->bus = NULL;
    }
    return &self->base;
}

/* ── 低层函数 ──────────────────────────────────────────── */

void ds18b20_start_conversion(DS18B20 *self)
{
    if (!self->bus) return;
    ow_reset(self->bus);
    ow_write_byte(self->bus, DS18B20_CMD_SKIP_ROM);
    ow_write_byte(self->bus, DS18B20_CMD_CONVERT_T);
}

void ds18b20_wait_conversion(DS18B20 *self)
{
    if (!self->bus) return;
    while (ow_read_bit(self->bus) == 0) {}
}

bool ds18b20_read_scratchpad(DS18B20 *self, uint8_t buf[9])
{
    if (!self->bus) return false;
    ow_reset(self->bus);
    ow_write_byte(self->bus, DS18B20_CMD_SKIP_ROM);
    ow_write_byte(self->bus, DS18B20_CMD_READ_SCRATCH);
    for (int i = 0; i < 9; i++) buf[i] = ow_read_byte(self->bus);
    return ow_crc8(buf, 9) == 0;
}

float ds18b20_parse_temp(const uint8_t sp[9])
{
    int16_t raw = (int16_t)((sp[1] << 8) | sp[0]);
    return (float)raw * 0.0625f;
}
