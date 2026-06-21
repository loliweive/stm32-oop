/** NVIC interrupt control — thin wrappers over CMSIS. */

#ifndef NVIC_HAL_H
#define NVIC_HAL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void NVIC_EnableIRQ(uint8_t irq);
void NVIC_DisableIRQ(uint8_t irq);
void NVIC_SetPriority(uint8_t irq, uint8_t priority);

/* STM32F103 IRQ numbers */
#define IRQ_USART1  37
#define IRQ_USART2  38
#define IRQ_USART3  39
#define IRQ_TIM2    28
#define IRQ_TIM3    29
#define IRQ_TIM4    30
#define IRQ_I2C1_EV 31
#define IRQ_I2C1_ER 32
#define IRQ_I2C2_EV 33
#define IRQ_I2C2_ER 34
#define IRQ_SPI1    35
#define IRQ_SPI2    36
#define IRQ_ADC1_2  18
#define IRQ_EXTI0   6
#define IRQ_EXTI1   7
#define IRQ_EXTI2   8
#define IRQ_EXTI3   9
#define IRQ_EXTI4   10
#define IRQ_EXTI9_5 23
#define IRQ_EXTI15_10 40

#ifdef __cplusplus
}
#endif

#endif /* NVIC_HAL_H */
