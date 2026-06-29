/**
 * @file    gpio.c
 * @brief   GPIO vtable — register (HOST_TEST/BAREMETAL) / HAL (FreeRTOS)
 */
#include "gpio.h"
#if defined(HOST_TEST) || defined(BAREMETAL)
#include "stm32f103xb.h"
#define GPIO_MODE_OUT_PP 0x03
#define GPIO_MODE_OUT_OD 0x07
#define GPIO_MODE_IN_PU 0x08
#else
#include "stm32f1xx_hal.h"
#include "stm32f1_compat.h"
#endif
static void _gpio_init(GpioPin *self) { self->_init = 1; gpio_set_mode(self, GPIO_MODE_OUT_PP); }

static void _gpio_set(GpioPin *self, uint8_t level) {
#if defined(HOST_TEST) || defined(BAREMETAL)
    GPIO_TypeDef *r = (GPIO_TypeDef *)self->port;
    if (level) r->BSRR = self->pin; else r->BRR = self->pin;
#else
    HAL_GPIO_WritePin((GPIO_TypeDef *)self->port, self->pin, level ? GPIO_PIN_SET : GPIO_PIN_RESET);
#endif
}

static uint8_t _gpio_get(GpioPin *self) {
#if defined(HOST_TEST) || defined(BAREMETAL)
    return (((GPIO_TypeDef *)self->port)->IDR & self->pin) ? 1 : 0;
#else
    return HAL_GPIO_ReadPin((GPIO_TypeDef *)self->port, self->pin) == GPIO_PIN_SET ? 1 : 0;
#endif
}

static void _gpio_toggle(GpioPin *self) {
#if defined(HOST_TEST) || defined(BAREMETAL)
    uint8_t c = _gpio_get(self); _gpio_set(self, !c);
#else
    HAL_GPIO_TogglePin((GPIO_TypeDef *)self->port, self->pin);
#endif
}

static void _gpio_set_mode(GpioPin *self, uint8_t mode) {
    if (self->pin == 0) return;
    GPIO_TypeDef *r = (GPIO_TypeDef *)self->port;
    uint32_t pos = 0;
    for (pos = 0; pos < 16; pos++) { if (self->pin & (1 << pos)) break; }
    if (pos >= 16) return;
    if (pos < 8) { uint32_t s = pos * 4; r->CRL = (r->CRL & ~(0xFUL << s)) | ((uint32_t)mode << s); }
    else { uint32_t s = (pos - 8) * 4; r->CRH = (r->CRH & ~(0xFUL << s)) | ((uint32_t)mode << s); }
    self->mode = mode;
}

static const GpioVtable _gpio_vtable = {
    .init = _gpio_init, .set = _gpio_set, .get = _gpio_get,
    .toggle = _gpio_toggle, .set_mode = _gpio_set_mode,
};

void GpioPin_ctor(GpioPin *self, void *port, uint16_t pin)
    { self->port = port; self->pin = pin; self->mode = 0; self->vtable = &_gpio_vtable; self->_init = 0; }
