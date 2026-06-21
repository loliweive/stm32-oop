/**
 * Host-compatible stub for CMSIS core_cm3.h.
 * Provides no-op implementations of ARM intrinsics for host testing.
 */
#ifndef CORE_CM3_HOST_H
#define CORE_CM3_HOST_H

#include <stdint.h>

/* NVIC / SysTick / SCB types — opaque on host */
typedef struct { uint32_t ISER[8], _r0[24], ICER[8], _r1[24], ISPR[8], _r2[24],
                  ICPR[8], _r3[24], IABR[8], _r4[56]; volatile uint8_t IP[240];
                  uint32_t _r5[644], STIR; } NVIC_Type;
typedef struct { volatile uint32_t CTRL, LOAD, VAL, CALIB; } SysTick_Type;

#define NVIC    ((NVIC_Type *)0xE000E100UL)
#define SysTick ((SysTick_Type *)0xE000E010UL)

#define SysTick_CTRL_ENABLE_Pos     0
#define SysTick_CTRL_ENABLE_Msk     (1UL << 0)
#define SysTick_CTRL_TICKINT_Pos    1
#define SysTick_CTRL_TICKINT_Msk    (1UL << 1)
#define SysTick_CTRL_CLKSOURCE_Pos  2
#define SysTick_CTRL_CLKSOURCE_Msk  (1UL << 2)

/* ARM intrinsics → host no-ops */
#define __NOP()     do {} while(0)
#define __WFI()     do {} while(0)
#define __WFE()     do {} while(0)
#define __SEV()     do {} while(0)
#define __DSB()     do {} while(0)
#define __ISB()     do {} while(0)
#define __DMB()     do {} while(0)
#define __disable_irq() do {} while(0)
#define __enable_irq()  do {} while(0)

#endif /* CORE_CM3_HOST_H */
