/**
 * Minimal CMSIS Core for Cortex-M3 — NVIC, SysTick, SCB definitions.
 * Sufficient for STM32F103C8T6 bare-metal development.
 */
#ifndef CORE_CM3_H
#define CORE_CM3_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* --- Memory-mapped core peripherals --- */
#define SCS_BASE            (0xE000E000UL)
#define SysTick_BASE        (SCS_BASE + 0x0010UL)
#define NVIC_BASE           (SCS_BASE + 0x0100UL)
#define SCB_BASE            (SCS_BASE + 0x0D00UL)

/* --- NVIC: Nested Vectored Interrupt Controller --- */
typedef struct {
    volatile uint32_t ISER[8];       /* 0x000: Interrupt Set Enable */
    uint32_t _reserved0[24];
    volatile uint32_t ICER[8];       /* 0x080: Interrupt Clear Enable */
    uint32_t _reserved1[24];
    volatile uint32_t ISPR[8];       /* 0x100: Interrupt Set Pending */
    uint32_t _reserved2[24];
    volatile uint32_t ICPR[8];       /* 0x180: Interrupt Clear Pending */
    uint32_t _reserved3[24];
    volatile uint32_t IABR[8];       /* 0x200: Interrupt Active Bit */
    uint32_t _reserved4[56];
    volatile uint8_t  IP[240];       /* 0x300: Interrupt Priority (byte-accessible) */
    uint32_t _reserved5[644];
    volatile uint32_t STIR;          /* 0xE00: Software Trigger Interrupt */
} NVIC_Type;

#define NVIC                ((NVIC_Type *)NVIC_BASE)

/* --- SysTick: System Timer --- */
typedef struct {
    volatile uint32_t CTRL;          /* Control & Status */
    volatile uint32_t LOAD;          /* Reload value */
    volatile uint32_t VAL;           /* Current value */
    volatile uint32_t CALIB;         /* Calibration (read-only) */
} SysTick_Type;

#define SysTick             ((SysTick_Type *)SysTick_BASE)

/* SysTick CTRL bits */
#define SysTick_CTRL_ENABLE_Pos   0
#define SysTick_CTRL_ENABLE_Msk   (1UL << 0)
#define SysTick_CTRL_TICKINT_Pos  1
#define SysTick_CTRL_TICKINT_Msk  (1UL << 1)
#define SysTick_CTRL_CLKSOURCE_Pos 2
#define SysTick_CTRL_CLKSOURCE_Msk (1UL << 2)
#define SysTick_CTRL_COUNTFLAG_Pos 16
#define SysTick_CTRL_COUNTFLAG_Msk (1UL << 16)

/* VTOR: Vector Table Offset Register (SCB) */
#define SCB_VTOR            (*(volatile uint32_t *)(SCB_BASE + 0x08))

/* --- Helpers --- */
#define __disable_irq()     __asm__ volatile("cpsid i" ::: "memory")
#define __enable_irq()      __asm__ volatile("cpsie i" ::: "memory")
#define __DSB()             __asm__ volatile("dsb 0xF" ::: "memory")
#define __ISB()             __asm__ volatile("isb 0xF" ::: "memory")
#define __DMB()             __asm__ volatile("dmb 0xF" ::: "memory")
#define __NOP()             __asm__ volatile("nop")
#define __WFI()             __asm__ volatile("wfi")
#define __WFE()             __asm__ volatile("wfe")
#define __SEV()             __asm__ volatile("sev")

void NVIC_EnableIRQ(uint8_t irq);
void NVIC_DisableIRQ(uint8_t irq);
void NVIC_SetPriority(uint8_t irq, uint8_t priority);

#ifdef __cplusplus
}
#endif

#endif /* CORE_CM3_H */
