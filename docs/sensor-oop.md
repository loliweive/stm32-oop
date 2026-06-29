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
typedef struct { Sensor base; I2cPort *i2c; uint8_t addr; } SHT30;

// 2. 实现 vtable
static bool sht30_read(Sensor *base, float *t, uint8_t *h) { ... }
static const SensorVtable sht30_vtable = {
    .read = sht30_read,
    .name = sht30_name, ...
};

// 3. 构造函数
Sensor *sht30_create(SHT30 *self, I2cPort *i2c, uint8_t addr) {
    self->base.vtable = &sht30_vtable;
    self->addr = addr;
    return &self->base;
}

// 4. 在 main.c 中注册
sensor = sht30_create(&sht30_obj, &i2c, 0x44);
// app 层 0 行改动 — sensor_read() 自动多态!
```
