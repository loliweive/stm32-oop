/**
 * @file    iwdg.c
 * @brief   IWDG 独立看门狗实现
 */
#include "iwdg.h"
#include <stdint.h>

/* IWDG registers (not in stm32f103xb.h) */
typedef struct { volatile uint32_t KR, PR, RLR, SR; } IWDG_TypeDef;
#define IWDG  ((IWDG_TypeDef *)0x40003000UL)
#define IWDG_SR_PVU  (1<<0)
#define IWDG_SR_RVU  (1<<1)

/* RCC CSR (not in headers we use) */
#define RCC_CSR_LSION  (1<<0)
#define RCC_CSR_LSIRDY (1<<1)
#define RCC_CSR        (*(volatile uint32_t *)0x40021024UL)

void iwdg_init(uint8_t pr, uint16_t rlr) {
    /* 开启 LSI (IWDG 的时钟源, 必须先启动) */
    RCC_CSR |= RCC_CSR_LSION;
    uint32_t to = 100000;
    while (!(RCC_CSR & RCC_CSR_LSIRDY) && --to) {}

    /* 解锁 PR 和 RLR 寄存器 */
    IWDG->KR = 0x5555;
    IWDG->PR = pr & 0x07;
    IWDG->RLR = rlr & 0x0FFF;
    for (volatile int i = 0; i < 1000; i++) __asm__("nop");  /* wait LSI sync */

    /* 启动 IWDG */
    IWDG->KR = 0xCCCC;
}

void iwdg_feed(void) {
    IWDG->KR = 0xAAAA;
}
