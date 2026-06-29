/**
 * @file    stm32f1xx_hal.h
 * @brief   Minimal HAL stub for host (x86) test compilation.
 *
 * Host tests use the DI (dependency injection) path — self->io != NULL.
 * These stubs are never called; they just need to compile.
 */
#ifndef STM32F1XX_HAL_HOST_H
#define STM32F1XX_HAL_HOST_H

#include "stm32f103xb_host.h"

typedef enum { HAL_OK = 0, HAL_ERROR = 1, HAL_BUSY = 2, HAL_TIMEOUT = 3 } HAL_StatusTypeDef;
typedef enum { GPIO_PIN_RESET = 0, GPIO_PIN_SET = 1 } GPIO_PinState;

#ifndef GPIO_MODE_OUT_PP
#define GPIO_MODE_OUT_PP       0x03
#endif
#ifndef GPIO_MODE_OUT_OD
#define GPIO_MODE_OUT_OD      0x07
#endif
#ifndef GPIO_MODE_IN_FL
#define GPIO_MODE_IN_FL       0x04
#endif
#ifndef GPIO_MODE_IN_PU
#define GPIO_MODE_IN_PU       0x08
#endif
#ifndef GPIO_MODE_OUTPUT_PP
#define GPIO_MODE_OUTPUT_PP   GPIO_MODE_OUT_PP
#endif
#ifndef GPIO_PIN_0
#define GPIO_PIN_0   ((uint16_t)0x0001)
#define GPIO_PIN_1   ((uint16_t)0x0002)
#define GPIO_PIN_2   ((uint16_t)0x0004)
#define GPIO_PIN_3   ((uint16_t)0x0008)
#define GPIO_PIN_4   ((uint16_t)0x0010)
#define GPIO_PIN_5   ((uint16_t)0x0020)
#define GPIO_PIN_6   ((uint16_t)0x0040)
#define GPIO_PIN_7   ((uint16_t)0x0080)
#define GPIO_PIN_8   ((uint16_t)0x0100)
#define GPIO_PIN_9   ((uint16_t)0x0200)
#define GPIO_PIN_10  ((uint16_t)0x0400)
#define GPIO_PIN_11  ((uint16_t)0x0800)
#define GPIO_PIN_12  ((uint16_t)0x1000)
#define GPIO_PIN_13  ((uint16_t)0x2000)
#define GPIO_PIN_14  ((uint16_t)0x4000)
#define GPIO_PIN_15  ((uint16_t)0x8000)
#endif

/* Stub HAL functions — never called in host tests (DI path) */
static inline void HAL_GPIO_WritePin(void *p, uint16_t n, GPIO_PinState s) { (void)p;(void)n;(void)s; }
static inline GPIO_PinState HAL_GPIO_ReadPin(void *p, uint16_t n) { (void)p;(void)n; return GPIO_PIN_RESET; }
static inline void HAL_GPIO_TogglePin(void *p, uint16_t n) { (void)p;(void)n; }

typedef struct { void *Instance; int dummy; } I2C_HandleTypeDef;
typedef struct { void *Instance; int dummy; } UART_HandleTypeDef;

static inline HAL_StatusTypeDef HAL_I2C_Master_Transmit(I2C_HandleTypeDef *h, uint16_t a, uint8_t *d, uint16_t s, uint32_t t) { (void)h;(void)a;(void)d;(void)s;(void)t; return HAL_OK; }
static inline HAL_StatusTypeDef HAL_I2C_Init(I2C_HandleTypeDef *h) { (void)h; return HAL_OK; }
static inline HAL_StatusTypeDef HAL_UART_Transmit(UART_HandleTypeDef *h, uint8_t *d, uint16_t s, uint32_t t) { (void)h;(void)d;(void)s;(void)t; return HAL_OK; }
static inline HAL_StatusTypeDef HAL_UART_Receive(UART_HandleTypeDef *h, uint8_t *d, uint16_t s, uint32_t t) { (void)h;(void)d;(void)s;(void)t; return HAL_OK; }

#define HAL_UART_STATE_READY 0x20
#define HAL_I2C_STATE_READY  0x20

#endif
