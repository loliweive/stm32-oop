/**
 * @file    onewire.h
 * @brief   1-Wire 总线驱动 — 基于 GPIO OOP vtable
 */
#ifndef ONEWIRE_H
#define ONEWIRE_H

#include <stdint.h>
#include <stdbool.h>
#include "gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    GpioPin    pin;       /**< GPIO pin (OOP vtable) */
    uint8_t    _init;     /**< 私有: 初始化标记 */
} OneWireBus;

void ow_init(OneWireBus *bus, void *port, uint16_t pin);
bool ow_reset(OneWireBus *bus);
void ow_write_bit(OneWireBus *bus, uint8_t bit);
uint8_t ow_read_bit(OneWireBus *bus);
void ow_write_byte(OneWireBus *bus, uint8_t byte);
uint8_t ow_read_byte(OneWireBus *bus);
int ow_search_rom(OneWireBus *bus, uint8_t rom[][8], int max);
uint8_t ow_crc8(const uint8_t *data, int len);

#ifdef __cplusplus
}
#endif

#endif /* ONEWIRE_H */
