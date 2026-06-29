/**
 * @file    gpio.c
 * @brief   GPIO vtable — HAL API internally, OOP vtable externally
 *
 * vtable 接口不变 (gpio_set/get/toggle/set_mode)。
 * 内部改用 STM32CubeF1 HAL API 替代直接寄存器操作。
 *   _gpio_set      → HAL_GPIO_WritePin
 *   _gpio_get      → HAL_GPIO_ReadPin
 *   _gpio_toggle   → HAL_GPIO_TogglePin
 *   _gpio_set_mode → 保持 CRL/CRH 直接写入 (raw 4-bit 编码与 HAL Mode enum 不兼容)
 *
 * HAL_GPIO_WritePin 内部也写 BSRR/BRR，原子性和原实现一致。
 */
#include "gpio.h"
#include "stm32f1xx_hal.h"

static GPIO_PinState _to_hal(uint8_t level) {
    return level ? GPIO_PIN_SET : GPIO_PIN_RESET;
}

static void _gpio_init(GpioPin *self)
{
    self->_init = 1;
    gpio_set_mode(self, GPIO_MODE_OUT_PP);
}

static void _gpio_set(GpioPin *self, uint8_t level)
{
    HAL_GPIO_WritePin((GPIO_TypeDef *)self->port, self->pin, _to_hal(level));
}

static uint8_t _gpio_get(GpioPin *self)
{
    return (HAL_GPIO_ReadPin((GPIO_TypeDef *)self->port, self->pin)
            == GPIO_PIN_SET) ? 1 : 0;
}

static void _gpio_toggle(GpioPin *self)
{
    HAL_GPIO_TogglePin((GPIO_TypeDef *)self->port, self->pin);
}

/**
 * @brief 设置引脚模式 — 保持 CRL/CRH 直接写入
 *
 * raw 4-bit 编码 (CNF[1:0]|MODE[1:0]) 与 HAL GPIO_InitTypeDef.Mode 编码
 * 完全不同 (GPIO_MODE_OUT_PP=0x03 RAW vs 0x10 in HAL)，
 * 转换表复杂且易出错。此函数仅在初始化时调用，非热路径。
 */
static void _gpio_set_mode(GpioPin *self, uint8_t mode)
{
    if (self->pin == 0) return;

    GPIO_TypeDef *reg = (GPIO_TypeDef *)self->port;
    uint32_t pos = 0;
    for (pos = 0; pos < 16; pos++) {
        if (self->pin & (1 << pos)) break;
    }
    if (pos >= 16) return;

    if (pos < 8) {
        uint32_t shift = pos * 4;
        reg->CRL = (reg->CRL & ~(0xFUL << shift))
                 | ((uint32_t)mode << shift);
    } else {
        uint32_t shift = (pos - 8) * 4;
        reg->CRH = (reg->CRH & ~(0xFUL << shift))
                 | ((uint32_t)mode << shift);
    }
    self->mode = mode;
}

static const GpioVtable _gpio_vtable = {
    .init     = _gpio_init,
    .set      = _gpio_set,
    .get      = _gpio_get,
    .toggle   = _gpio_toggle,
    .set_mode = _gpio_set_mode,
};

void GpioPin_ctor(GpioPin *self, void *port, uint16_t pin)
{
    self->port   = port;
    self->pin    = pin;
    self->mode   = 0;
    self->vtable = &_gpio_vtable;
    self->_init  = 0;
}
