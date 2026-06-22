/**
 * @file    freertos_hooks.c
 * @brief   FreeRTOS 钩子函数实现 (栈溢出、内存分配失败等)
 *
 * 这些钩子由 FreeRTOS 内核在异常情况下调用。
 * 在生产代码中应该记录错误并进入安全状态。
 * 在这里我们使用断言机制 — 开发阶段立即停止，
 * 发布阶段 (NDEBUG) 可配置为软复位或忽略。
 */

#include "FreeRTOS.h"
#include "task.h"

/**
 * @brief 栈溢出钩子
 *
 * 当 configCHECK_FOR_STACK_OVERFLOW=1 时，
 * FreeRTOS 在任务切换时检查栈指针是否越界。
 * 如果检测到溢出，调用此函数。
 *
 * 栈溢出的常见原因：
 *   1. configMINIMAL_STACK_SIZE 设置太小
 *   2. 函数局部变量太大 (大数组在栈上)
 *   3. 递归调用过深
 *   4. 中断嵌套过深
 *
 * 修复方法：增大问题任务的栈大小。
 */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    (void)pcTaskName;

    /* 在开发阶段：断言失败，立即停止 */
    /* 在发布阶段：可记录错误，软复位 MCU */

    /* SCB AIRCR: 系统复位 (Application Interrupt and Reset Control Register)
       VECTKEY = 0x05FA (写保护密钥, 高16位)
       SYSRESETREQ (bit 2) = 请求系统复位 */
    #define AIRCR_ADDR            ((volatile uint32_t *)0xE000ED0CUL)
    #define AIRCR_VECTKEY         (0x05FAUL << 16)
    #define AIRCR_SYSRESETREQ     (1UL << 2)
    *AIRCR_ADDR = AIRCR_VECTKEY | AIRCR_SYSRESETREQ;
    while (1) {
        __asm__ volatile("nop");
    }
}

/**
 * @brief 内存分配失败钩子
 *
 * 当 configUSE_MALLOC_FAILED_HOOK=1 时调用。
 * 当前未启用 (堆内存不足时会返回 NULL 而非调用此钩子)。
 */
#if 0
void vApplicationMallocFailedHook(void)
{
    /* 进入无限循环 — 开发阶段使用断言更合适 */
    while (1) { __asm__ volatile("nop"); }
}
#endif
