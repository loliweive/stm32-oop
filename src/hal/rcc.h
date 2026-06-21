/** Clock tree configuration for STM32F103C8T6. */

#ifndef RCC_H
#define RCC_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    RCC_HSE = 0,
    RCC_HSI,
    RCC_PLL,
} RccClockSource;

typedef struct {
    uint32_t sysclk_hz;
    uint32_t hclk_hz;
    uint32_t pclk1_hz;  /* APB1 (max 36MHz) */
    uint32_t pclk2_hz;  /* APB2 (max 72MHz) */
} RccConfig;

/**
 * Configure system clock.
 *
 * Typical: HSE=8MHz → PLL×9 → SYSCLK=72MHz
 *   rcc_set_sysclk(RCC_HSE, 9);
 */
void rcc_set_sysclk(RccClockSource src, uint8_t pll_mul);
void rcc_get_config(RccConfig *cfg);

/* Peripheral clock enable */
void rcc_enable_gpio(char port);   /* 'A', 'B', 'C' */
void rcc_enable_usart(uint8_t n);  /* 1, 2, 3 */
void rcc_enable_timer(uint8_t n);  /* 1..4 */
void rcc_enable_i2c(uint8_t n);    /* 1, 2 */
void rcc_enable_spi(uint8_t n);    /* 1, 2 */
void rcc_enable_adc(uint8_t n);    /* 1 */

#ifdef __cplusplus
}
#endif

#endif /* RCC_H */
