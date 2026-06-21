/**
 * Host-compatible stub for stm32f103xb.h.
 * Provides register types and macros used by HAL code during host testing.
 * Actual register access is mocked; this supplies the type definitions.
 */
#ifndef STM32F103XB_HOST_H
#define STM32F103XB_HOST_H

#include <stdint.h>

/* ── Register types (opaque on host) ────────────────────────── */
typedef struct { uint32_t CRL, CRH, IDR, ODR, BSRR, BRR, LCKR; } GPIO_Type;
typedef struct { uint32_t SR, DR, BRR, CR1, CR2, CR3, GTPR; } USART_Type;
typedef struct { uint32_t CR, CFGR, CIR, APB2RSTR, APB1RSTR, AHBENR, APB2ENR, APB1ENR, BDCR, CSR; } RCC_Type;
typedef struct { uint32_t CR1, CR2, SMCR, DIER, SR, EGR, CCMR1, CCMR2, CCER, CNT, PSC, ARR, _r1, CCR[4]; } TIM_Type;
typedef struct { uint32_t CR1, CR2, OAR1, OAR2, DR, SR1, SR2, CCR, TRISE; } I2C_Type;
typedef struct { uint32_t CR1, CR2, SR, DR, CRCPR, RXCRCR, TXCRCR; } SPI_Type;
typedef struct { uint32_t SR, CR1, CR2, SMPR1, SMPR2, JOFR[4], HTR, LTR, SQR1, SQR2, SQR3, JSQR, JDR[4], DR; } ADC_Type;
typedef struct { volatile uint32_t ACR; } FLASH_Type;

/* ── GPIO pin masks ─────────────────────────────────────────── */
#define GPIO_PIN_0  (1 << 0)
#define GPIO_PIN_1  (1 << 1)
#define GPIO_PIN_2  (1 << 2)
#define GPIO_PIN_3  (1 << 3)
#define GPIO_PIN_4  (1 << 4)
#define GPIO_PIN_5  (1 << 5)
#define GPIO_PIN_6  (1 << 6)
#define GPIO_PIN_7  (1 << 7)
#define GPIO_PIN_8  (1 << 8)
#define GPIO_PIN_9  (1 << 9)
#define GPIO_PIN_10 (1 << 10)
#define GPIO_PIN_11 (1 << 11)
#define GPIO_PIN_12 (1 << 12)
#define GPIO_PIN_13 (1 << 13)
#define GPIO_PIN_14 (1 << 14)
#define GPIO_PIN_15 (1 << 15)

/* ── GPIO mode macros ───────────────────────────────────────── */
#define GPIO_MODE_OUT_PP    0x33
#define GPIO_CNF_PP         0x00
#define GPIO_MODE_OUT_50MHZ 0x03

/* ── USART macros ───────────────────────────────────────────── */
#define USART_SR_TXE        (1 << 7)
#define USART_SR_RXNE       (1 << 5)
#define USART_SR_TC         (1 << 6)
#define USART_CR1_UE        (1 << 13)
#define USART_CR1_TE        (1 << 3)
#define USART_CR1_RE        (1 << 2)

/* ── RCC register macros ────────────────────────────────────── */
#define RCC_APB2ENR_IOPAEN  (1 << 2)
#define RCC_APB2ENR_IOPBEN  (1 << 3)
#define RCC_APB2ENR_IOPCEN  (1 << 4)
#define RCC_APB2ENR_USART1EN (1 << 14)
#define RCC_APB2ENR_AFIOEN  (1 << 0)
#define RCC_APB1ENR_USART2EN (1 << 17)
#define RCC_APB1ENR_TIM2EN  (1 << 0)
#define RCC_APB1ENR_TIM3EN  (1 << 1)
#define RCC_APB1ENR_TIM4EN  (1 << 2)
#define RCC_APB1ENR_I2C1EN  (1 << 21)
#define RCC_APB1ENR_I2C2EN  (1 << 22)
#define RCC_APB1ENR_SPI2EN  (1 << 14)

#define RCC_CR_HSEON        (1 << 16)
#define RCC_CR_HSERDY       (1 << 17)
#define RCC_CR_PLLON        (1 << 24)
#define RCC_CR_PLLRDY       (1 << 25)
#define RCC_CFGR_SW_PLL     2
#define RCC_CFGR_SW_HSE     1
#define RCC_CFGR_SWS_Msk    (3 << 2)
#define RCC_CFGR_PLLSRC     (1 << 16)
#define RCC_CFGR_PLLMUL_Msk (0xF << 18)
#define RCC_CFGR_PLLMUL(n)  (((n)-2) << 18)

/* ── FLASH macros ───────────────────────────────────────────── */
#define FLASH_ACR_PRFTBE    (1 << 4)
#define FLASH_ACR_LATENCY_2 (2 << 0)

/* ── TIM macros ──────────────────────────────────────────────── */
#define TIM_CR1_CEN         (1 << 0)
#define TIM_CR1_ARPE        (1 << 7)
#define TIM_SR_UIF          (1 << 0)

/* ── STM32F103 peripheral base addresses (used in tests) ──────── */
#define GPIOA ((GPIO_Type *)0x40010800)
#define GPIOB ((GPIO_Type *)0x40010C00)
#define GPIOC ((GPIO_Type *)0x40011000)
#define USART1 ((USART_Type *)0x40013800)
#define USART2 ((USART_Type *)0x40004400)
#define RCC    ((RCC_Type *)0x40021000)
#define FLASH  ((FLASH_Type *)0x40022000)

#endif /* STM32F103XB_HOST_H */
