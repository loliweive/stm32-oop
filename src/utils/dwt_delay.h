/**
 * @file    dwt_delay.h
 * @brief   DWT CYCCNT 精确定时 — 替代 volatile 空循环
 *
 * 要求: Cortex-M3+ (DWT 单元), 系统时钟 72MHz。
 * 使用 CMSIS core_cm3.h 的 CoreDebug/DWT 宏, 不用原始地址。
 *
 * 用法:
 *   #include "dwt_delay.h"
 *   DWT_DELAY_MS(3);   // 精确 3ms @ 72MHz
 */

#ifndef DWT_DELAY_H
#define DWT_DELAY_H

#include "stm32f103xb.h"  /* device header → pulls in core_cm3.h with IRQn_Type */

/* ── DWT 初始化 (幂等, 内联) ─────────────────────────────── */
#define DWT_INIT() do { \
    static int __dwt_init = 0; \
    if (!__dwt_init) { \
        CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk; \
        DWT->CYCCNT = 0; \
        DWT->CTRL  |= DWT_CTRL_CYCCNTENA_Msk; \
        __dwt_init = 1; \
    } \
} while(0)

/* ── 毫秒延迟 (72MHz → 72,000 cycles/ms) ────────────────── */
#define DWT_DELAY_MS(ms) do { \
    DWT_INIT(); \
    uint32_t __t = DWT->CYCCNT + (uint32_t)(ms) * 72000UL; \
    while ((int32_t)(DWT->CYCCNT - __t) < 0) {} \
} while(0)

#endif /* DWT_DELAY_H */
