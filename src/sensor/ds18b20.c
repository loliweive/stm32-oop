/**
 * @file    ds18b20.c
 * @brief   DS18B20 驱动实现
 *
 * 温度数据格式 (12-bit):
 *   MSB                         LSB
 *   S S S S S 2^6 2^5 2^4      2^3 2^2 2^1 2^0 2^-1 2^-2 2^-3 2^-4
 *
 *   符号位 S: 0 = 正温, 1 = 负温 (补码)
 *   例: 0x0191 = 25.0625°C
 *       0xFE6F = -25.0625°C (补码)
 */
#include "ds18b20.h"

bool ds18b20_init(DS18B20 *sensor, OneWireBus *bus)
{
    sensor->bus     = bus;
    sensor->present = false;

    /* 读 ROM — 单设备场景 */
    if (ow_reset(bus)) {
        ow_write_byte(bus, DS18B20_CMD_READ_ROM);
        for (int i = 0; i < 8; i++) {
            sensor->rom[i] = ow_read_byte(bus);
        }
        /* 验证 CRC8 */
        if (ow_crc8(sensor->rom, 8) == 0) {
            sensor->present = true;
            return true;
        }
        /* CRC 失败但设备应答 — 可能多设备, 尝试 SKIP ROM */
        sensor->present = true;
        return true;
    }

    return false;
}

void ds18b20_start_conversion(DS18B20 *sensor)
{
    ow_reset(sensor->bus);
    ow_write_byte(sensor->bus, DS18B20_CMD_SKIP_ROM);
    ow_write_byte(sensor->bus, DS18B20_CMD_CONVERT_T);
}

void ds18b20_wait_conversion(DS18B20 *sensor)
{
    /* 转换期间设备将总线拉低。读时隙返回 0 = 忙, 1 = 完成 */
    while (1) {
        uint8_t busy = ow_read_bit(sensor->bus);
        if (busy == 1) break;
    }
}

bool ds18b20_read_scratchpad(DS18B20 *sensor, uint8_t buf[9])
{
    ow_reset(sensor->bus);
    ow_write_byte(sensor->bus, DS18B20_CMD_SKIP_ROM);
    ow_write_byte(sensor->bus, DS18B20_CMD_READ_SCRATCH);

    for (int i = 0; i < 9; i++) {
        buf[i] = ow_read_byte(sensor->bus);
    }

    /* CRC8 验证 */
    return ow_crc8(buf, 9) == 0;
}

bool ds18b20_read_temp(DS18B20 *sensor, float *temp_c)
{
    uint8_t buf[9];

    ds18b20_start_conversion(sensor);
    ds18b20_wait_conversion(sensor);

    if (!ds18b20_read_scratchpad(sensor, buf)) {
        return false;
    }

    *temp_c = ds18b20_parse_temp(buf);
    return true;
}

float ds18b20_parse_temp(const uint8_t scratchpad[9])
{
    int16_t raw = (int16_t)((scratchpad[1] << 8) | scratchpad[0]);

    /* 12-bit 精度: LSB = 0.0625°C */
    return (float)raw * 0.0625f;
}
