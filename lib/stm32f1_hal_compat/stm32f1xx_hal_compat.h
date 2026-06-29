/**
 * @file    stm32f1xx_hal_compat.h
 * @brief   STM32Cube HAL API 兼容层 — 零 ROM 开销
 *
 * 实现 STM32 HAL 标准函数签名，内部直接操作寄存器。
 * 目的: 上层代码使用 HAL API，换 STM32F4 时替换底层寄存器操作即可。
 *
 * 使用方法:
 *   #include "stm32f1xx_hal_compat.h"
 *   HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET); // LED ON
 *
 * 对比完整 STM32 HAL:
 *   完整 HAL: stm32f1xx_hal_gpio.c (20KB) → ROM +3KB
 *   兼容层:   本文件 (~100 行 inline) → ROM 0 (编译器优化为直接寄存器写)
 */
#ifndef STM32F1XX_HAL_COMPAT_H
#define STM32F1XX_HAL_COMPAT_H

#include "stm32f103xb.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── HAL 状态码 ──────────────────────────────────────────── */
typedef enum { HAL_OK = 0, HAL_ERROR = 1, HAL_BUSY = 2, HAL_TIMEOUT = 3 } HAL_StatusTypeDef;

#define HAL_MAX_DELAY 0xFFFFFFFFUL

/* ── 类型别名 (STM32 HAL → 本项目 CMSIS) ─────────────────── */
typedef GPIO_Type  GPIO_TypeDef;
typedef USART_Type USART_TypeDef;
typedef I2C_Type   I2C_TypeDef;
typedef SPI_Type   SPI_TypeDef;

/* ═══════════════════════════════════════════════════════════
 * GPIO
 * ═══════════════════════════════════════════════════════════ */

typedef enum { GPIO_PIN_RESET = 0, GPIO_PIN_SET = 1 } GPIO_PinState;

typedef struct {
    uint32_t Pin;       /* GPIO pin mask (e.g. GPIO_PIN_13) */
    uint32_t Mode;      /* GPIO mode (not used in compat layer) */
    uint32_t Pull;      /* Pull-up/down (not used in compat layer) */
    uint32_t Speed;     /* Output speed (not used in compat layer) */
} GPIO_InitTypeDef;

/* atomic write via BSRR/BRR — single STR instruction */
static inline void HAL_GPIO_WritePin(GPIO_Type *GPIOx, uint16_t Pin,
                                     GPIO_PinState s) {
    if (s) GPIOx->BSRR = Pin;
    else   GPIOx->BRR  = Pin;
}

static inline GPIO_PinState HAL_GPIO_ReadPin(GPIO_Type *GPIOx,
                                             uint16_t Pin) {
    return (GPIOx->IDR & Pin) ? GPIO_PIN_SET : GPIO_PIN_RESET;
}

static inline void HAL_GPIO_TogglePin(GPIO_Type *GPIOx, uint16_t Pin) {
    /* ODR flip — single LDR+EOR+STR (3 instructions) */
    uint32_t odr = GPIOx->ODR;
    GPIOx->BSRR = ((odr & Pin) << 16) | (~odr & Pin);
}

/* init stub — our RCC driver handles clock; this is for compatibility */
static inline void HAL_GPIO_Init(GPIO_Type *GPIOx, GPIO_InitTypeDef *cfg) {
    (void)GPIOx; (void)cfg; /* GPIO clock assumed already enabled */
}

/* ═══════════════════════════════════════════════════════════
 * RCC (clock control stubs — actual init in our rcc.c)
 * ═══════════════════════════════════════════════════════════ */

static inline void HAL_RCC_GPIOA_CLK_ENABLE(void) { RCC->APB2ENR |= RCC_APB2ENR_IOPAEN; }
static inline void HAL_RCC_GPIOB_CLK_ENABLE(void) { RCC->APB2ENR |= RCC_APB2ENR_IOPBEN; }
static inline void HAL_RCC_GPIOC_CLK_ENABLE(void) { RCC->APB2ENR |= RCC_APB2ENR_IOPCEN; }
static inline void HAL_RCC_USART1_CLK_ENABLE(void){ RCC->APB2ENR |= RCC_APB2ENR_USART1EN; }
static inline void HAL_RCC_SPI1_CLK_ENABLE(void)  { RCC->APB2ENR |= RCC_APB2ENR_SPI1EN; }
static inline void HAL_RCC_I2C1_CLK_ENABLE(void)  { RCC->APB1ENR |= RCC_APB1ENR_I2C1EN; }
static inline void HAL_RCC_ADC1_CLK_ENABLE(void)  { RCC->APB2ENR |= RCC_APB2ENR_ADC1EN; }

/* ═══════════════════════════════════════════════════════════
 * UART
 * ═══════════════════════════════════════════════════════════ */

typedef struct {
    USART_Type      *Instance;
    uint32_t         BaudRate;
    uint32_t         WordLength;
    uint32_t         StopBits;
    uint32_t         Parity;
    uint32_t         Mode;
} UART_HandleTypeDef;

#define HAL_UART_STATE_READY 0x20U

static inline HAL_StatusTypeDef HAL_UART_Init(UART_HandleTypeDef *huart) {
    USART_TypeDef *u = huart->Instance;
    uint32_t pclk = 36000000; /* APB2 = 72MHz/2 */
    u->BRR = (uint16_t)((pclk + huart->BaudRate/2) / huart->BaudRate);
    u->CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;
    return HAL_OK;
}

static inline HAL_StatusTypeDef HAL_UART_Transmit(UART_HandleTypeDef *huart,
    const uint8_t *data, uint16_t size, uint32_t timeout) {
    (void)timeout;
    for (uint16_t i = 0; i < size; i++) {
        while (!(huart->Instance->SR & USART_SR_TXE)) {}
        huart->Instance->DR = data[i];
    }
    return HAL_OK;
}

static inline HAL_StatusTypeDef HAL_UART_Receive(UART_HandleTypeDef *huart,
    uint8_t *data, uint16_t size, uint32_t timeout) {
    uint32_t to = timeout;
    for (uint16_t i = 0; i < size; i++) {
        while (!(huart->Instance->SR & USART_SR_RXNE) && --to) {}
        if (!to) return HAL_TIMEOUT;
        data[i] = (uint8_t)huart->Instance->DR;
    }
    return HAL_OK;
}

/* ═══════════════════════════════════════════════════════════
 * I2C
 * ═══════════════════════════════════════════════════════════ */

typedef struct {
    I2C_Type        *Instance;
    uint32_t         Timing;       /* not used — compat only */
    uint32_t         OwnAddress1;
    uint32_t         AddressingMode;
    uint32_t         DualAddressMode;
} I2C_HandleTypeDef;

static inline HAL_StatusTypeDef HAL_I2C_Master_Transmit(I2C_HandleTypeDef *hi2c,
    uint16_t DevAddress, const uint8_t *data, uint16_t size, uint32_t timeout) {
    I2C_TypeDef *i = hi2c->Instance;
    uint32_t to = timeout * 1000; /* rough: ~1 cycle per count */
    /* START */
    i->CR1 |= I2C_CR1_START;
    while (!(i->SR1 & I2C_SR1_SB) && --to) {}
    if (!to) return HAL_TIMEOUT;
    /* ADDR */
    i->DR = (uint8_t)(DevAddress << 1);
    while (!(i->SR1 & I2C_SR1_ADDR) && --to) {}
    (void)i->SR2; /* clear ADDR */
    if (!to) { i->CR1 |= I2C_CR1_STOP; return HAL_TIMEOUT; }
    /* DATA */
    for (uint16_t n = 0; n < size; n++) {
        while (!(i->SR1 & I2C_SR1_TXE) && --to) {}
        if (!to) { i->CR1 |= I2C_CR1_STOP; return HAL_TIMEOUT; }
        i->DR = data[n];
    }
    /* STOP */
    while (!(i->SR1 & I2C_SR1_BTF) && --to) {}
    i->CR1 |= I2C_CR1_STOP;
    return HAL_OK;
}

/* ═══════════════════════════════════════════════════════════
 * SPI
 * ═══════════════════════════════════════════════════════════ */

typedef struct {
    SPI_Type        *Instance;
    uint32_t         Mode;
    uint32_t         Direction;
    uint32_t         DataSize;
    uint32_t         CLKPolarity;
    uint32_t         CLKPhase;
    uint32_t         BaudRatePrescaler;
    uint32_t         FirstBit;
} SPI_HandleTypeDef;

static inline uint8_t HAL_SPI_TransmitReceive(SPI_HandleTypeDef *hspi,
    const uint8_t *tx, uint8_t *rx, uint16_t size, uint32_t timeout) {
    (void)timeout;
    if (rx) *rx = 0;
    for (uint16_t i = 0; i < size; i++) {
        while (!(hspi->Instance->SR & SPI_SR_TXE)) {}
        hspi->Instance->DR = tx ? tx[i] : 0xFF;
        while (!(hspi->Instance->SR & SPI_SR_RXNE)) {}
        uint8_t r = (uint8_t)hspi->Instance->DR;
        if (rx) rx[i] = r;
    }
    return HAL_OK;
}

static inline HAL_StatusTypeDef HAL_SPI_Transmit(SPI_HandleTypeDef *hspi,
    const uint8_t *data, uint16_t size, uint32_t timeout) {
    return HAL_SPI_TransmitReceive(hspi, data, NULL, size, timeout);
}

/* ═══════════════════════════════════════════════════════════
 * System Tick (for HAL_Delay compatibility)
 * ═══════════════════════════════════════════════════════════ */

extern volatile uint32_t uwTick; /* incremented by SysTick ISR */

static inline uint32_t HAL_GetTick(void) { return uwTick; }

static inline void HAL_Delay(uint32_t delay_ms) {
    uint32_t start = HAL_GetTick();
    while ((HAL_GetTick() - start) < delay_ms) {}
}

static inline HAL_StatusTypeDef HAL_Init(void) { return HAL_OK; }

#ifdef __cplusplus
}
#endif

#endif /* STM32F1XX_HAL_COMPAT_H */
