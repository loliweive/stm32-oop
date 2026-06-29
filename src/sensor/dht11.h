/**
 * @file    dht11.h
 * @brief   DHT11 温湿度 — Sensor OOP vtable
 */
#ifndef DHT11_H
#define DHT11_H

#include "sensor.h"
#include "gpio.h"

typedef struct {
    Sensor   base;
    GpioPin  pin;            /**< GPIO pin (OOP vtable → HAL API) */
    uint8_t  humidity;
    uint8_t  temp_c;
} DHT11;

Sensor *dht11_create(DHT11 *self, void *port, uint16_t pin);
bool dht11_raw_read(DHT11 *self);

#endif
