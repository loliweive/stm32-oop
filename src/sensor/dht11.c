/**
 * @file    dht11.c
 * @brief   DHT11 — Sensor OOP vtable, GPIO via GpioPin (HAL API)
 */
#include "dht11.h"
#include "stm32f1xx_hal.h"

/* ── DWT µs delay ─────────────────────────────────────── */
static void _dwt_delay_us(uint32_t us) {
    uint32_t t = *(volatile uint32_t*)0xE0001004 + us * 72;
    while ((int32_t)(*(volatile uint32_t*)0xE0001004 - t) < 0) {}
}
#define DELAY_US(us) do { \
    static int _init = 0; \
    if (!_init) { \
        *(volatile uint32_t*)0xE000EDFC |= (1<<24); \
        *(volatile uint32_t*)0xE0001004 = 0; \
        *(volatile uint32_t*)0xE0001000 |= 1; \
        _init = 1; \
    } \
    _dwt_delay_us(us); \
} while(0)

/* ── GPIO helpers (all via OOP vtable → HAL API) ────── */
static void _pin_out(DHT11 *d) { gpio_set_mode(&d->pin, GPIO_MODE_OUT_OD); }
static void _pin_in(DHT11 *d)  { gpio_set_mode(&d->pin, GPIO_MODE_IN_PU); }
static void _pin_lo(DHT11 *d)  { gpio_set(&d->pin, 0); }
static void _pin_hi(DHT11 *d)  { gpio_set(&d->pin, 1); }
static uint8_t _pin_rd(DHT11 *d) { return gpio_get(&d->pin); }

/* ── 低层读取 ────────────────────────────────────────── */

bool dht11_raw_read(DHT11 *dht)
{
    uint8_t data[5] = {0};

    __disable_irq();
    _pin_out(dht); _pin_lo(dht); DELAY_US(18000);
    _pin_hi(dht);  DELAY_US(30);
    _pin_in(dht);

    uint32_t to = 10000;
    while (_pin_rd(dht)==1 && --to){} if(!to){__enable_irq();return false;}
    DELAY_US(80);
    to=10000; while (_pin_rd(dht)==0&&--to){} if(!to){__enable_irq();return false;}
    DELAY_US(80);

    for(int b=0;b<5;b++) for(int i=0;i<8;i++){
        to=1000; while (_pin_rd(dht)==0&&--to){}
        DELAY_US(40);
        data[b]<<=1; if(_pin_rd(dht)) data[b]|=1;
        to=1000; while (_pin_rd(dht)==1&&--to){}
    }
    __enable_irq();

    if ((uint8_t)(data[0]+data[1]+data[2]+data[3]) != data[4]) return false;
    if (data[0]==0 && data[1]==0 && data[2]==0 && data[3]==0 && data[4]==0)
        return false;

    dht->temp_c  = data[2];
    dht->humidity = data[0];
    return true;
}

/* ── Sensor vtable ──────────────────────────────────── */

static bool _read(Sensor *base, float *temp_c, uint8_t *humidity)
{
    DHT11 *self = (DHT11 *)base;
    if (!dht11_raw_read(self)) {
        if (temp_c) *temp_c = -273.0f;
        if (humidity) *humidity = 255;
        return false;
    }
    if (temp_c) *temp_c   = (float)self->temp_c;
    if (humidity) *humidity = self->humidity;
    return true;
}

static const char *_name(Sensor *base) { (void)base; return "DHT11"; }
static bool _is_present(Sensor *base) { return ((DHT11 *)base)->pin.pin != 0; }

static bool _init(Sensor *base)
{
    DHT11 *self = (DHT11 *)base;
    _pin_out(self); _pin_hi(self);
    return true;
}

static const SensorVtable dht11_vtable = {
    .read       = _read,
    .name       = _name,
    .is_present = _is_present,
    .init       = _init,
};

Sensor *dht11_create(DHT11 *self, void *port, uint16_t pin)
{
    self->base.vtable = &dht11_vtable;
    GpioPin_ctor(&self->pin, port, pin);
    self->temp_c = 0;
    self->humidity = 0;
    _init(&self->base);
    return &self->base;
}
