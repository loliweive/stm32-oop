/**
 * @file    stm32f1_compat.h
 * @brief   Values missing from STM32 HAL headers but needed by our code.
 */
#ifndef STM32F1_COMPAT_H
#define STM32F1_COMPAT_H

/* Raw GPIO 4-bit mode encodings for CRL/CRH writes.
   HAL's GPIO_MODE_OUTPUT_PP=0x10 is a different encoding — DO NOT USE for CRL/CRH! */
#define GPIO_MODE_OUT_PP       0x03   /* push-pull 50MHz */
#define GPIO_MODE_OUT_OD      0x07   /* open-drain 50MHz */
#define GPIO_MODE_IN_FL       0x04   /* floating input */
#define GPIO_MODE_IN_PU       0x08   /* pull-up input */

/* GPIO pin masks (our code uses these; STM32 HAL does not define them) */
#define GPIO_PIN_0    ((uint16_t)0x0001)
#define GPIO_PIN_1    ((uint16_t)0x0002)
#define GPIO_PIN_2    ((uint16_t)0x0004)
#define GPIO_PIN_3    ((uint16_t)0x0008)
#define GPIO_PIN_4    ((uint16_t)0x0010)
#define GPIO_PIN_5    ((uint16_t)0x0020)
#define GPIO_PIN_6    ((uint16_t)0x0040)
#define GPIO_PIN_7    ((uint16_t)0x0080)
#define GPIO_PIN_8    ((uint16_t)0x0100)
#define GPIO_PIN_9    ((uint16_t)0x0200)
#define GPIO_PIN_10   ((uint16_t)0x0400)
#define GPIO_PIN_11   ((uint16_t)0x0800)
#define GPIO_PIN_12   ((uint16_t)0x1000)
#define GPIO_PIN_13   ((uint16_t)0x2000)
#define GPIO_PIN_14   ((uint16_t)0x4000)
#define GPIO_PIN_15   ((uint16_t)0x8000)

/* Legacy aliases */
#define GPIO_MODE_OUT_50MHZ   GPIO_MODE_OUT_PP
#define GPIO_MODE_IN          GPIO_MODE_IN_FL
#endif /* STM32F1_COMPAT_H */
