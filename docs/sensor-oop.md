# Sensor OOP 类继承图

## vtable 多态架构

```
                    ┌─────────────────────────┐
                    │     Sensor (abstract)     │
                    │  vtable:                  │
                    │    read(temp, humidity)   │
                    │    name() → "DS18B20"     │
                    │    is_present() → bool    │
                    │    init() → bool          │
                    └────────────┬──────────────┘
                                 │ "extends"
              ┌──────────────────┼──────────────────┐
              │                  │                  │
     ┌────────┴────────┐ ┌──────┴──────┐ ┌────────┴────────┐
     │   DS18B20       │ │   DHT11     │ │  LightSensor    │
     │ base: Sensor    │ │ base:Sensor │ │ base: Sensor    │
     │ bus: OneWireBus │ │ port: GPIOA │ │ adc: ADC1       │
     │ rom[8]          │ │ pin: PA1    │ │ adc_ch: 3(PA3)  │
     └────────┬────────┘ └──────┬──────┘ │ do_port: GPIOB  │
              │                 │        │ do_pin: PB11     │
              │ uses            │        └─────────────────┘
     ┌────────┴────────┐        │
     │  OneWireBus      │        │ (GPIO bit-bang)
     │ port: GPIOA      │        │
     │ pin: PA1         │        │
     └─────────────────┘        │
```

## 调用链

```
App:  sensor_read(s, &temp, &hum)
        │
        │  self->vtable->read(self, temp, hum)
        │
  ┌─────┴────────────────────────────────┐
  │  DS18B20._read()                     │  DHT11._read()
  │    ds18b20_start_conversion()        │    dht11_raw_read()
  │    ds18b20_wait_conversion()  750ms  │    GPIO bit-bang 25ms
  │    ds18b20_read_scratchpad()         │    checksum verify
  │    ds18b20_parse_temp()              │    temp_c + humidity
  │    CRC8 verify                       │
  └──────────────────────────────────────┘
```

## 添加新传感器

```c
// 1. 继承 Sensor
typedef struct { Sensor base; I2cPort *i2c; } BMP280;

// 2. 实现 vtable
static bool bmp280_read(Sensor *base, float *t, uint8_t *h) { ... }
static const SensorVtable bmp280_vtable = {
    .read = bmp280_read,
    .name = bmp280_name, ...
};

// 3. 构造函数
Sensor *bmp280_create(BMP280 *self, I2cPort *i2c) {
    self->base.vtable = &bmp280_vtable;
    return &self->base;
}

// 4. 在 main.c 中注册
sensor = bmp280_create(&bmp280_obj, &i2c);
// app 层 0 行改动 — sensor_read() 自动多态!
```
