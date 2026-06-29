/**
 * @file    onewire.c
 * @brief   1-Wire 总线 — GPIO OOP vtable (HAL API internally)
 */
#include "onewire.h"
#include "stm32f1xx_hal.h"

/* ── DWT µs delay ─────────────────────────────────────── */
static void _dwt_delay_us(uint32_t us) {
    uint32_t t = *(volatile uint32_t*)0xE0001004 + us * 72;
    while ((int32_t)(*(volatile uint32_t*)0xE0001004 - t) < 0) {}
}
#define DELAY_US(us) do { \
    static int _dwt_init = 0; \
    if (!_dwt_init) { \
        *(volatile uint32_t*)0xE000EDFC |= (1<<24); \
        *(volatile uint32_t*)0xE0001004 = 0; \
        *(volatile uint32_t*)0xE0001000 |= 1; \
        _dwt_init = 1; \
    } \
    _dwt_delay_us(us); \
} while(0)

/* All GPIO ops go through OOP vtable (→ HAL_GPIO_* internally) */

void ow_init(OneWireBus *bus, void *port, uint16_t pin)
{
    GpioPin_ctor(&bus->pin, port, pin);
    bus->_init = 1;
}

bool ow_reset(OneWireBus *bus)
{
    uint8_t presence;
    __disable_irq();
    gpio_set_mode(&bus->pin, GPIO_MODE_OUT_OD); /* open-drain output */
    gpio_set(&bus->pin, 0);
    DELAY_US(480);
    gpio_set_mode(&bus->pin, GPIO_MODE_IN_PU); /* input pull-up */
    gpio_set(&bus->pin, 1);                     /* release bus */
    DELAY_US(70);
    presence = gpio_get(&bus->pin);
    __enable_irq();
    DELAY_US(410);
    return (presence == 0);
}

void ow_write_bit(OneWireBus *bus, uint8_t bit)
{
    __disable_irq();
    gpio_set_mode(&bus->pin, GPIO_MODE_OUT_OD);
    gpio_set(&bus->pin, 0);
    DELAY_US(2);
    if (bit) {
        gpio_set_mode(&bus->pin, GPIO_MODE_IN_PU);
        gpio_set(&bus->pin, 1);
        DELAY_US(58);
    } else {
        DELAY_US(58);
        gpio_set_mode(&bus->pin, GPIO_MODE_IN_PU);
        gpio_set(&bus->pin, 1);
    }
    __enable_irq();
    DELAY_US(1);
}

uint8_t ow_read_bit(OneWireBus *bus)
{
    uint8_t bit;
    __disable_irq();
    gpio_set_mode(&bus->pin, GPIO_MODE_OUT_OD);
    gpio_set(&bus->pin, 0);
    DELAY_US(2);
    gpio_set_mode(&bus->pin, GPIO_MODE_IN_PU);
    gpio_set(&bus->pin, 1);
    DELAY_US(8);
    bit = gpio_get(&bus->pin);
    __enable_irq();
    DELAY_US(50);
    return bit;
}

void ow_write_byte(OneWireBus *bus, uint8_t byte)
{
    for (int i = 0; i < 8; i++) {
        ow_write_bit(bus, byte & 0x01);
        byte >>= 1;
    }
}

uint8_t ow_read_byte(OneWireBus *bus)
{
    uint8_t byte = 0;
    for (int i = 0; i < 8; i++) {
        byte >>= 1;
        if (ow_read_bit(bus)) byte |= 0x80;
    }
    return byte;
}

uint8_t ow_crc8(const uint8_t *data, int len)
{
    uint8_t crc = 0;
    while (len--) {
        uint8_t byte = *data++;
        for (int i = 0; i < 8; i++) {
            uint8_t mix = (crc ^ byte) & 0x01;
            crc >>= 1;
            if (mix) crc ^= 0x8C;
            byte >>= 1;
        }
    }
    return crc;
}

int ow_search_rom(OneWireBus *bus, uint8_t rom[][8], int max)
{
    int found = 0;
    if (max >= 1 && ow_reset(bus)) {
        ow_write_byte(bus, 0x33);
        for (int i = 0; i < 8; i++) {
            rom[0][i] = ow_read_byte(bus);
        }
        if (ow_crc8(rom[0], 8) == 0) found = 1;
    }
    return found;
}
