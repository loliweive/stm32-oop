/**
 * Precise delay using SysTick or TIM loop.
 * Non-blocking variants use system tick counter.
 */

#ifndef DELAY_H
#define DELAY_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Initialize the delay subsystem (configures SysTick). */
void delay_init(void);

/** Busy-wait for `ms` milliseconds (SysTick-based). */
void delay_ms(uint32_t ms);

/** Busy-wait for `us` microseconds (instruction loop). */
void delay_us(uint32_t us);

/** Get system tick in milliseconds (since boot). */
uint32_t millis(void);

#ifdef __cplusplus
}
#endif

#endif /* DELAY_H */
