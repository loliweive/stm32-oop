/**
 * Minimal STM32F103xB peripheral register definitions.
 * Covers RCC, GPIO, USART, TIM, I2C, SPI, ADC, AFIO, FLASH.
 * Only 64KB Flash variant (STM32F103C8T6) register map.
 */
#ifndef STM32F103XB_H
#define STM32F103XB_H

#include <stdint.h>
#include "core_cm3.h"

/* --- Base addresses --- */
#define PERIPH_BASE         0x40000000UL
#define APB1_PERIPH_BASE    (PERIPH_BASE)
#define APB2_PERIPH_BASE    (PERIPH_BASE + 0x10000)
#define AHBPERIPH_BASE      (PERIPH_BASE + 0x20000)

/* FLASH */
#define FLASH_R_BASE        (AHBPERIPH_BASE + 0x2000)
#define FLASH_BASE          FLASH_R_BASE

/* RCC */
#define RCC_BASE            (AHBPERIPH_BASE + 0x1000)

/* GPIO */
#define GPIOA_BASE          (APB2_PERIPH_BASE + 0x0800)
#define GPIOB_BASE          (APB2_PERIPH_BASE + 0x0C00)
#define GPIOC_BASE          (APB2_PERIPH_BASE + 0x1000)

/* USART */
#define USART1_BASE         (APB2_PERIPH_BASE + 0x3800)
#define USART2_BASE         (APB1_PERIPH_BASE + 0x4400)
#define USART3_BASE         (APB1_PERIPH_BASE + 0x4800)

/* TIM */
#define TIM1_BASE           (APB2_PERIPH_BASE + 0x2C00)
#define TIM2_BASE           (APB1_PERIPH_BASE + 0x0000)
#define TIM3_BASE           (APB1_PERIPH_BASE + 0x0400)
#define TIM4_BASE           (APB1_PERIPH_BASE + 0x0800)

/* I2C */
#define I2C1_BASE           (APB1_PERIPH_BASE + 0x5400)
#define I2C2_BASE           (APB1_PERIPH_BASE + 0x5800)

/* SPI */
#define SPI1_BASE           (APB2_PERIPH_BASE + 0x3000)
#define SPI2_BASE           (APB1_PERIPH_BASE + 0x3800)

/* ADC */
#define ADC1_BASE           (APB2_PERIPH_BASE + 0x2400)

/* AFIO */
#define AFIO_BASE           (APB2_PERIPH_BASE + 0x0000)

/* --- FLASH --- */
typedef struct {
    volatile uint32_t ACR;
    volatile uint32_t KEYR;
    volatile uint32_t OPTKEYR;
    volatile uint32_t SR;
    volatile uint32_t CR;
    volatile uint32_t AR;
    uint32_t _reserved;
    volatile uint32_t OBR;
    volatile uint32_t WRPR;
} FLASH_Type;

#define FLASH               ((FLASH_Type *)FLASH_BASE)

#define FLASH_ACR_LATENCY_0  (0 << 0)
#define FLASH_ACR_LATENCY_1  (1 << 0)
#define FLASH_ACR_LATENCY_2  (2 << 0)
#define FLASH_ACR_PRFTBE     (1 << 4)

/* --- RCC: Reset and Clock Control --- */
typedef struct {
    volatile uint32_t CR;
    volatile uint32_t CFGR;
    volatile uint32_t CIR;
    volatile uint32_t APB2RSTR;
    volatile uint32_t APB1RSTR;
    volatile uint32_t AHBENR;
    volatile uint32_t APB2ENR;
    volatile uint32_t APB1ENR;
    volatile uint32_t BDCR;
    volatile uint32_t CSR;
} RCC_Type;

#define RCC                 ((RCC_Type *)RCC_BASE)

/* RCC_CR bits */
#define RCC_CR_HSION        (1 << 0)
#define RCC_CR_HSIRDY       (1 << 1)
#define RCC_CR_HSEON        (1 << 16)
#define RCC_CR_HSERDY       (1 << 17)
#define RCC_CR_HSEBYP       (1 << 18)
#define RCC_CR_PLLON        (1 << 24)
#define RCC_CR_PLLRDY       (1 << 25)

/* RCC_CFGR bits */
#define RCC_CFGR_SW_HSI     0
#define RCC_CFGR_SW_HSE     1
#define RCC_CFGR_SW_PLL     2
#define RCC_CFGR_SWS_Msk    (3 << 2)
#define RCC_CFGR_PLLSRC     (1 << 16)  /* 0=HSI/2, 1=HSE */
#define RCC_CFGR_PLLMUL_Msk (0xF << 18)
#define RCC_CFGR_PLLMUL(n)  (((n)-2) << 18)  /* 2..16 */

/* RCC_AHBENR */
#define RCC_AHBENR_DMA1EN   (1 << 0)
#define RCC_AHBENR_FLITFEN  (1 << 4)
#define RCC_AHBENR_CRCEN    (1 << 6)

/* RCC_APB2ENR */
#define RCC_APB2ENR_AFIOEN  (1 << 0)
#define RCC_APB2ENR_IOPAEN  (1 << 2)
#define RCC_APB2ENR_IOPBEN  (1 << 3)
#define RCC_APB2ENR_IOPCEN  (1 << 4)
#define RCC_APB2ENR_ADC1EN  (1 << 9)
#define RCC_APB2ENR_TIM1EN  (1 << 11)
#define RCC_APB2ENR_SPI1EN  (1 << 12)
#define RCC_APB2ENR_USART1EN (1 << 14)

/* RCC_APB1ENR */
#define RCC_APB1ENR_TIM2EN  (1 << 0)
#define RCC_APB1ENR_TIM3EN  (1 << 1)
#define RCC_APB1ENR_TIM4EN  (1 << 2)
#define RCC_APB1ENR_I2C1EN  (1 << 21)
#define RCC_APB1ENR_I2C2EN  (1 << 22)
#define RCC_APB1ENR_SPI2EN  (1 << 14)
#define RCC_APB1ENR_USART2EN (1 << 17)
#define RCC_APB1ENR_USART3EN (1 << 18)

/* --- GPIO --- */
typedef struct {
    volatile uint32_t CRL;
    volatile uint32_t CRH;
    volatile uint32_t IDR;
    volatile uint32_t ODR;
    volatile uint32_t BSRR;
    volatile uint32_t BRR;
    volatile uint32_t LCKR;
} GPIO_Type;

#define GPIOA               ((GPIO_Type *)GPIOA_BASE)
#define GPIOB               ((GPIO_Type *)GPIOB_BASE)
#define GPIOC               ((GPIO_Type *)GPIOC_BASE)

/* GPIO mode (CNF[1:0] << 2 | MODE[1:0]) */
#define GPIO_MODE_IN        0x0   /* 00: Input */
#define GPIO_MODE_OUT_10MHZ 0x1   /* 01: Output 10MHz */
#define GPIO_MODE_OUT_2MHZ  0x2   /* 10: Output 2MHz */
#define GPIO_MODE_OUT_50MHZ 0x3   /* 11: Output 50MHz */
#define GPIO_CNF_ANALOG     (0x0 << 2)
#define GPIO_CNF_FLOAT      (0x1 << 2)
#define GPIO_CNF_PP         (0x0 << 2)  /* Push-pull (output) */
#define GPIO_CNF_OD         (0x1 << 2)  /* Open-drain (output) */
#define GPIO_CNF_ALT_PP     (0x2 << 2)  /* Alt function push-pull */
#define GPIO_CNF_ALT_OD     (0x3 << 2)  /* Alt function open-drain */

#define GPIO_MODE_OUT_PP    (GPIO_CNF_PP | GPIO_MODE_OUT_50MHZ)
#define GPIO_MODE_OUT_OD    (GPIO_CNF_OD | GPIO_MODE_OUT_50MHZ)

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

/* --- USART --- */
typedef struct {
    volatile uint32_t SR;
    volatile uint32_t DR;
    volatile uint32_t BRR;
    volatile uint32_t CR1;
    volatile uint32_t CR2;
    volatile uint32_t CR3;
    volatile uint32_t GTPR;
} USART_Type;

#define USART1              ((USART_Type *)USART1_BASE)
#define USART2              ((USART_Type *)USART2_BASE)
#define USART3              ((USART_Type *)USART3_BASE)

/* USART_SR */
#define USART_SR_TXE        (1 << 7)
#define USART_SR_TC         (1 << 6)
#define USART_SR_RXNE       (1 << 5)

/* USART_CR1 */
#define USART_CR1_UE        (1 << 13)
#define USART_CR1_TE        (1 << 3)
#define USART_CR1_RE        (1 << 2)
#define USART_CR1_RXNEIE    (1 << 5)

/* --- AFIO --- */
typedef struct {
    volatile uint32_t EVCR;
    volatile uint32_t MAPR;
    volatile uint32_t EXTICR[4];
    volatile uint32_t MAPR2;
} AFIO_Type;

#define AFIO                ((AFIO_Type *)AFIO_BASE)

/* --- Timer --- */
typedef struct {
    volatile uint32_t CR1;
    volatile uint32_t CR2;
    volatile uint32_t SMCR;
    volatile uint32_t DIER;
    volatile uint32_t SR;
    volatile uint32_t EGR;
    volatile uint32_t CCMR1;
    volatile uint32_t CCMR2;
    volatile uint32_t CCER;
    volatile uint32_t CNT;
    volatile uint32_t PSC;
    volatile uint32_t ARR;
    volatile uint32_t _reserved1;
    volatile uint32_t CCR[4];
} TIM_Type;

#define TIM1                ((TIM_Type *)TIM1_BASE)
#define TIM2                ((TIM_Type *)TIM2_BASE)
#define TIM3                ((TIM_Type *)TIM3_BASE)
#define TIM4                ((TIM_Type *)TIM4_BASE)

/* TIM_CR1 */
#define TIM_CR1_CEN         (1 << 0)
#define TIM_CR1_UDIS        (1 << 1)
#define TIM_CR1_URS         (1 << 2)
#define TIM_CR1_ARPE        (1 << 7)

/* TIM_SR */
#define TIM_SR_UIF          (1 << 0)

/* TIM_DIER */
#define TIM_DIER_UIE        (1 << 0)

/* --- I2C --- */
typedef struct {
    volatile uint32_t CR1;
    volatile uint32_t CR2;
    volatile uint32_t OAR1;
    volatile uint32_t OAR2;
    volatile uint32_t DR;
    volatile uint32_t SR1;
    volatile uint32_t SR2;
    volatile uint32_t CCR;
    volatile uint32_t TRISE;
} I2C_Type;

#define I2C1                ((I2C_Type *)I2C1_BASE)
#define I2C2                ((I2C_Type *)I2C2_BASE)

/* --- SPI --- */
typedef struct {
    volatile uint32_t CR1;
    volatile uint32_t CR2;
    volatile uint32_t SR;
    volatile uint32_t DR;
    volatile uint32_t CRCPR;
    volatile uint32_t RXCRCR;
    volatile uint32_t TXCRCR;
} SPI_Type;

#define SPI1                ((SPI_Type *)SPI1_BASE)
#define SPI2                ((SPI_Type *)SPI2_BASE)

/* --- ADC --- */
typedef struct {
    volatile uint32_t SR;
    volatile uint32_t CR1;
    volatile uint32_t CR2;
    volatile uint32_t SMPR1;
    volatile uint32_t SMPR2;
    volatile uint32_t JOFR[4];
    volatile uint32_t HTR;
    volatile uint32_t LTR;
    volatile uint32_t SQR1;
    volatile uint32_t SQR2;
    volatile uint32_t SQR3;
    volatile uint32_t JSQR;
    volatile uint32_t JDR[4];
    volatile uint32_t DR;
} ADC_Type;

#define ADC1                ((ADC_Type *)ADC1_BASE)

#endif /* STM32F103XB_H */
