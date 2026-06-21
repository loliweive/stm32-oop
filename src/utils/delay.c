/**
 * @file    delay.c
 * @brief   精确延时 + 系统时钟 — 双模式 (裸机 / FreeRTOS)
 *
 * 裸机模式：
 *   使用 SysTick 硬件定时器生成 1ms 中断。
 *   SysTick_Handler 由本文件实现。
 *
 * FreeRTOS 模式 (USE_FREERTOS 已定义)：
 *   SysTick 完全交给 FreeRTOS 内核管理 (port.c)。
 *   delay_ms → vTaskDelay (阻塞延时, 让出 CPU)
 *   millis  → xTaskGetTickCount (FreeRTOS tick 计数)
 *   SysTick_Handler 不在此文件中定义 (由 port.c 提供)。
 */

#include "delay.h"
#include "core_cm3.h"


/* ═══════════════════════════════════════════════════════════════
 *  裸机模式 — SysTick 硬件驱动
 * ═══════════════════════════════════════════════════════════════ */

#ifndef USE_FREERTOS

static volatile uint32_t _ticks;

void delay_init(void)
{
    /* 配置 SysTick: 72MHz / 72000 = 1ms 周期 */
    SysTick->LOAD = 72000 - 1;
    SysTick->VAL  = 0;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk   /* 使用 CPU 时钟 */
                  | SysTick_CTRL_TICKINT_Msk     /* 启用中断 */
                  | SysTick_CTRL_ENABLE_Msk;     /* 启动定时器 */
    _ticks = 0;
}

void delay_ms(uint32_t ms)
{
    /* 裸机忙等待 (非阻塞 WFI) — 每次 SysTick 中断唤醒 */
    uint32_t start = _ticks;
    while ((_ticks - start) < ms) {
        __WFI(); /* 睡眠直到下一个中断 */
    }
}

void delay_us(uint32_t us)
{
    /* 粗略的指令循环延时
       72MHz / 12 cycles ≈ 167ns/次 → us * 6 ≈ 1µs
       注意：中断会延长延时 */
    uint32_t count = us * 6;
    while (count--) {
        __NOP();
    }
}

uint32_t millis(void)
{
    return _ticks;
}

/* SysTick 中断处理 — 裸机模式 */


void SysTick_Handler(void)
{
    _ticks++;
}

#else  /* USE_FREERTOS — 使用 FreeRTOS 的 SysTick */

#include "FreeRTOS.h"
#include "task.h"

void delay_init(void)
{
    /* FreeRTOS 在 vTaskStartScheduler() 中初始化 SysTick，无需手动配置 */
}

void delay_ms(uint32_t ms)
{
    /* 阻塞延时 — 让出 CPU 给其他任务
       裸机的 delay_ms 是忙等待, FreeRTOS 的是任务切换 */
    vTaskDelay(pdMS_TO_TICKS(ms));
}

void delay_us(uint32_t us)
{
    /* FreeRTOS 下微秒延时仍用指令循环 (任务切换开销太大)
       对于 < 1ms 的短延时，指令循环比任务切换更精确 */
    uint32_t count = us * 6;
    while (count--) {
        __NOP();
    }
}

uint32_t millis(void)
{
    /* FreeRTOS tick 计数 (1 tick = 1ms, 由 configTICK_RATE_HZ=1000 决定) */
    return (uint32_t)xTaskGetTickCount();
}

/* SysTick_Handler 由 FreeRTOS 的 port.c 提供，此处不定义 */

#endif /* USE_FREERTOS */

