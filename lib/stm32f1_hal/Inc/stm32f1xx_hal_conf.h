/**
 * @file    stm32f1xx_hal_conf.h
 * @brief   STM32CubeF1 HAL configuration for STM32F103C8T6 (64KB Flash)
 *
 * Only enables modules this project uses to minimize binary size.
 */
#ifndef STM32F1XX_HAL_CONF_H
#define STM32F1XX_HAL_CONF_H

#ifdef __cplusplus
extern "C" {
#endif

#define USE_HAL_DRIVER
#define HAL_MODULE_ENABLED
#define HAL_GPIO_MODULE_ENABLED
#define HAL_UART_MODULE_ENABLED
#define HAL_I2C_MODULE_ENABLED
#define HAL_SPI_MODULE_ENABLED
#define HAL_ADC_MODULE_ENABLED
#define HAL_TIM_MODULE_ENABLED
#define HAL_DMA_MODULE_ENABLED
#define HAL_RCC_MODULE_ENABLED
#define HAL_CORTEX_MODULE_ENABLED
#define HAL_FLASH_MODULE_ENABLED

/* HSE oscillator: 8MHz external crystal */
#define HSE_VALUE    8000000UL
#define HSE_STARTUP_TIMEOUT 100
#define LSE_STARTUP_TIMEOUT 5000

/* HSI oscillator: 8MHz internal RC */
#define HSI_VALUE    8000000UL

/* SYSCLK = 72MHz (HSE x9 PLL) */
#define HSE_PREDIV2_ENABLED   0U
#define HSE_PLL_MUL            9U

/* Tick frequency for HAL_Delay */
#define TICK_INT_PRIORITY 0U
#define USE_RTOS          0U   /* FreeRTOS manages SysTick, HAL tick unused */

/* Assert */
#define assert_param(expr) ((void)0U)

/* CMSIS compatibility definitions (standard STM32 HAL requires these) */
#define __IO    volatile
#define __I     volatile const
#define __O     volatile
#define __WEAK  __attribute__((weak))
/* __NVIC_PRIO_BITS is already in stm32f103xb.h, don't redefine */

#include "stm32f1_compat.h" /* raw GPIO encodings — before HAL to avoid redef conflicts */
#include "stm32f1xx_hal_def.h"  /* HAL common definitions (HAL_StatusTypeDef etc) */
#include "stm32f1xx.h"  /* device header → select stm32f103xb.h */
/* Include HAL peripheral headers (type definitions).
   Order matters: DMA before UART/I2C/SPI (they reference DMA_HandleTypeDef). */
#ifdef HAL_DMA_MODULE_ENABLED
#include "stm32f1xx_hal_dma.h"
#endif
#ifdef HAL_RCC_MODULE_ENABLED
#include "stm32f1xx_hal_rcc.h"
#endif
#ifdef HAL_GPIO_MODULE_ENABLED
#include "stm32f1xx_hal_gpio.h"
#endif
#ifdef HAL_UART_MODULE_ENABLED
#include "stm32f1xx_hal_uart.h"
#endif
#ifdef HAL_I2C_MODULE_ENABLED
#include "stm32f1xx_hal_i2c.h"
#endif
#ifdef HAL_SPI_MODULE_ENABLED
#include "stm32f1xx_hal_spi.h"
#endif
#ifdef HAL_ADC_MODULE_ENABLED
#include "stm32f1xx_hal_adc.h"
#endif
#ifdef HAL_TIM_MODULE_ENABLED
#include "stm32f1xx_hal_tim.h"
#endif
#ifdef HAL_CORTEX_MODULE_ENABLED
#include "stm32f1xx_hal_cortex.h"
#endif
#ifdef HAL_FLASH_MODULE_ENABLED
#include "stm32f1xx_hal_flash.h"
#endif
#ifdef __cplusplus
}
#endif

#endif /* STM32F1XX_HAL_CONF_H */
