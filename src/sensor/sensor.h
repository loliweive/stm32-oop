/**
 * @file    sensor.h
 * @brief   Sensor 抽象接口 — OOP in C (vtable 多态)
 *
 * 所有传感器实现此接口，应用层通过 Sensor* 统一操作，
 * 无需关心底层是 DS18B20 / DHT11 / BMP280 / ...
 *
 * ── 类继承结构 ──────────────────────────────────────────
 *
 *        Sensor (abstract)
 *        │  vtable: read(), name(), is_present()
 *        │
 *   ┌────┴──────────────┐
 *   DS18B20             DHT11
 *   (OneWire)           (单总线 GPIO)
 *
 * ── 使用方法 ────────────────────────────────────────────
 *
 *   // 应用层只持有 Sensor 指针, 不关心具体类型:
 *   Sensor *s = ds18b20_create(&ds18b20_obj, &bus);
 *
 *   float temp; uint8_t hum;
 *   if (sensor_read(s, &temp, &hum)) {
 *       printf("%s: %.2f C\n", sensor_name(s), temp);
 *   }
 *
 * ── 对比原来的代码 ─────────────────────────────────────
 *
 *   原来: 每种传感器单独处理
 *     #if SENSOR_TYPE == DS18B20
 *       ds18b20_read(&sensor, &temp);
 *     #elif SENSOR_TYPE == DHT11
 *       dht11_read(&sensor);
 *       temp = dht11_get_temp(&sensor);
 *     #endif
 *
 *   现在: 运行时多态, 无需 #if
 *     sensor_read(s, &temp, &hum);
 */
#ifndef SENSOR_H
#define SENSOR_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── 前向声明 ──────────────────────────────────────────── */
typedef struct Sensor Sensor;

/**
 * @brief Sensor 虚函数表
 *
 * 实现一个新的传感器只需要实现这 4 个函数。
 */
typedef struct {
    /**
     * @brief 读取传感器数据
     * @param self     传感器实例
     * @param temp_c   输出: 摄氏度 (−273 表示无效)
     * @param humidity 输出: %RH (0-100, 255 表示不支持)
     * @return true 读取成功
     */
    bool (*read)(Sensor *self, float *temp_c, uint8_t *humidity);

    /** @brief 传感器名称 ("DS18B20", "DHT11"...) */
    const char *(*name)(Sensor *self);

    /** @brief 设备是否在线/可用 */
    bool (*is_present)(Sensor *self);

    /**
     * @brief 初始化传感器 (上电后调用一次)
     * @return true 设备就绪
     */
    bool (*init)(Sensor *self);
} SensorVtable;

/**
 * @brief Sensor 基类
 *
 * 每个具体传感器结构体的第一个成员必须是 Sensor base。
 * 类似 C++ 的继承: struct DS18B20 : Sensor { ... };
 */
struct Sensor {
    const SensorVtable *vtable;
};

/* ── 内联调度器 (类似 HAL 层的 gpio_set/gpio_get) ──────── */

/** @brief 读取传感器, 返回温度(°C)和湿度(%RH, 255=不支持) */
static inline bool sensor_read(Sensor *self, float *temp_c, uint8_t *humidity) {
    return self->vtable->read(self, temp_c, humidity);
}

/** @brief 获取传感器名称 */
static inline const char *sensor_name(Sensor *self) {
    return self->vtable->name(self);
}

/** @brief 设备是否在线 */
static inline bool sensor_is_present(Sensor *self) {
    return self->vtable->is_present(self);
}

/** @brief 初始化传感器 */
static inline bool sensor_init(Sensor *self) {
    return self->vtable->init(self);
}

#ifdef __cplusplus
}
#endif

#endif /* SENSOR_H */
