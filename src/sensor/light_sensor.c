/**
 * @file    light_sensor.c
 * @brief   光敏传感器实现 — Sensor vtable + ADC + GPIO
 */
#include "light_sensor.h"
#include "stm32f1xx_hal.h"
#include "adc.h"
#include "gpio.h"

/* ── vtable 实现 ────────────────────────────────────────── */

static bool _read(Sensor *base, float *lux_pct, uint8_t *digital)
{
    LightSensor *self = (LightSensor *)base;

    /* ── 模拟量: ADC 读 PA3 (不重新初始化, _init 已做) ── */
    AdcPort adc_port;
    AdcPort_ctor(&adc_port, self->adc);
    /* adc_init 只在 _init 调用一次, 这里直接读 */

    uint16_t raw = adc_read(&adc_port, self->adc_channel);
    self->lux_pct = (float)raw * 100.0f / 4095.0f;

    /* ── 数字量: GPIO 读 PB11 ─────────────────────────── */
    GpioPin dpin;
    GpioPin_ctor(&dpin, self->do_port, self->do_pin);
    gpio_set_mode(&dpin, 0x04 | 0x0);  /* 浮空输入 */
    self->digital = gpio_get(&dpin);

    if (lux_pct)  *lux_pct  = self->lux_pct;
    if (digital)  *digital  = self->digital;
    return true;
}

static const char *_name(Sensor *base)    { (void)base; return "Light(M7)"; }
static bool _is_present(Sensor *base)     { (void)base; return true; }

static bool _init(Sensor *base)
{
    LightSensor *self = (LightSensor *)base;

    /* PA3 模拟输入 (ADC1_IN3) */
    GpioPin ain;
    GpioPin_ctor(&ain, GPIOA, GPIO_PIN_3);
    gpio_set_mode(&ain, 0x00 /* analog mode */ | GPIO_MODE_IN);

    /* 初始化 ADC */
    AdcPort adc_port;
    AdcPort_ctor(&adc_port, self->adc);
    adc_init(&adc_port);

    /* 初始化数字输入 (PB11, 上拉) */
    GpioPin dpin;
    GpioPin_ctor(&dpin, self->do_port, self->do_pin);
    gpio_set_mode(&dpin, 0x8 | GPIO_MODE_IN);
    gpio_set(&dpin, 1);

    return true;
}

static const SensorVtable light_vtable = {
    .read       = _read,
    .name       = _name,
    .is_present = _is_present,
    .init       = _init,
};

Sensor *light_sensor_create(LightSensor *self, void *adc, uint8_t adc_channel,
                            void *do_port, uint16_t do_pin)
{
    self->base.vtable = &light_vtable;
    self->adc         = adc;
    self->adc_channel = adc_channel;
    self->do_port     = do_port;
    self->do_pin      = do_pin;
    self->lux_pct     = 0;
    self->digital     = 0;
    _init(&self->base);
    return &self->base;
}
