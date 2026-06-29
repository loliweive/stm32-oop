/**
 * System initialization for STM32F103C8T6.
 * Configures HSE (8MHz external crystal) → PLL (×9) → 72MHz SYSCLK.
 */
#include "stm32f103xb.h"

/* Default: HSI 8MHz — PLL disabled until configured by RCC driver */
void SystemInit(void)
{
    /* Enable HSI, set as SYSCLK: default after reset */
    /* CSP, prefetch buffer enable — required for >48MHz */
    FLASH->ACR = FLASH_ACR_PRFTBE | FLASH_ACR_LATENCY_2;
}
