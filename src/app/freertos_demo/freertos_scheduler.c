/**
 * @file    freertos_scheduler.c
 * @brief   FreeRTOS scheduler adapter implementation
 *
 * 简单的封装层，主要目的是：
 *   1. 将 TaskParams 结构体映射到 xTaskCreate 的参数
 *   2. 统一错误处理
 *   3. 为未来扩展预留位置 (如静态分配任务、运行统计等)
 */

#include "freertos_scheduler.h"
#include "FreeRTOS.h"
#include "task.h"

/**
 * @brief 创建任务
 *
 * 注意：FreeRTOS 的栈大小以 words 为单位，不是 bytes。
 * 对于 Cortex-M3 (32-bit)，1 word = 4 bytes。
 *
 * 例如：stack_words=128 → 512 bytes 栈空间。
 *
 * 推荐的栈大小 (经验值)：
 *   简单任务 (LED 闪烁):   128 words = 512 bytes
 *   中等任务 (UART 处理):  256 words = 1024 bytes
 *   复杂任务 (协议栈):     512+ words = 2048+ bytes
 *
 * 可以用 uxTaskGetStackHighWaterMark() 在运行时监控
 * 实际栈使用情况，然后调整。
 */
BaseType_t task_create(const TaskParams *p)
{
    return xTaskCreate(
        p->func,          /* 任务入口函数 */
        p->name,          /* 任务名 (调试用) */
        p->stack_words,   /* 栈深度 (words) */
        p->params,        /* 参数指针 */
        p->priority,      /* 优先级 */
        p->handle         /* 句柄输出 (可为 NULL) */
    );
}

/**
 * @brief 启动调度器 — 永不返回
 *
 * vTaskStartScheduler() 会：
 *   1. 创建 idle 任务 (最低优先级, 永远就绪)
 *   2. 如果 configUSE_TIMERS=1，创建 timer 任务
 *   3. 初始化 SysTick 为 configTICK_RATE_HZ
 *   4. 触发 SVC 异常，在异常返回时跳到最高优先级任务
 *
 * 如果启动失败 (通常是 configTOTAL_HEAP_SIZE 太小)，
 * 会返回。此时进入死循环 (更好的做法是软复位)。
 */
void freertos_start(void)
{
    vTaskStartScheduler();

    /* 调度器启动失败 — 不应该到这里 */
    while (1) {
        __asm__ volatile("nop");
    }
}
