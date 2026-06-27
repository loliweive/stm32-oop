/**
 * @file    FreeRTOSConfig.h
 * @brief   FreeRTOS configuration for STM32F103C8T6 (64KB Flash / 20KB SRAM)
 *
 * 每个 FreeRTOS 项目必须有这个文件。它定义了内核的行为参数：
 *   - 时钟频率和 tick 速率
 *   - 堆大小 (从 20KB SRAM 中分配)
 *   - 启用的功能 (互斥锁、信号量、队列等)
 *   - 中断优先级映射 (STM32 用 4 bits = 16 个优先级)
 *
 * 关键约束：
 *   - Flash: 64KB  (FreeRTOS 内核约 6-8KB)
 *   - SRAM:  20KB  (堆配 6KB, 剩余栈+数据)
 *   - Cortex-M3: configKERNEL_INTERRUPT_PRIORITY = 255 (最低)
 */

#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#include "stm32f103xb.h"

/* ── 中断处理器名称映射 ────────────────────────────────────── */
/* FreeRTOS port.c defines vPortSVCHandler/xPortPendSVHandler/xPortSysTickHandler,
 * but the startup vector table uses CMSIS standard names.
 * These #defines rename the FreeRTOS handlers to match the vector table. */
#define vPortSVCHandler         SVC_Handler
#define xPortPendSVHandler      PendSV_Handler
#define xPortSysTickHandler     SysTick_Handler

/* ── 时钟 ──────────────────────────────────────────────────── */
#define configCPU_CLOCK_HZ                      (72000000UL)  /* 72MHz PLL — must match actual clock! */
#define configTICK_RATE_HZ                      ((TickType_t)1000)  /* 1ms tick */

/* ── Tick 类型 ──────────────────────────────────────────────── */
#define configUSE_16_BIT_TICKS                  0  /* 32-bit tick counter */

/* ── 任务调度 ──────────────────────────────────────────────── */
#define configMAX_PRIORITIES                    (5)
#define configMINIMAL_STACK_SIZE                ((uint16_t)128)     /* words, not bytes */
#define configMAX_TASK_NAME_LEN                 (12)
#define configUSE_PREEMPTION                    1
#define configUSE_TIME_SLICING                  1
#define configIDLE_SHOULD_YIELD                 1

/* ── 内存 ──────────────────────────────────────────────────── */
#define configTOTAL_HEAP_SIZE                   ((size_t)(8 * 1024)) /* 8KB heap — CLI 6KB + idle 512B + TCBs */
#define configSUPPORT_DYNAMIC_ALLOCATION        1
#define configSUPPORT_STATIC_ALLOCATION         0

/* ── 同步原语 ──────────────────────────────────────────────── */
#define configUSE_MUTEXES                       1
#define configUSE_COUNTING_SEMAPHORES           1
#define configUSE_RECURSIVE_MUTEXES             0
#define configQUEUE_REGISTRY_SIZE               5

/* ── 调试与统计 ──────────────────────────────────────────────── */
#define configUSE_TRACE_FACILITY                1
#define configUSE_STATS_FORMATTING_FUNCTIONS    1
#define configSTATS_BUFFER_MAX_LENGTH           256

/* ── 可选 API 包含 ───────────────────────────────────────────── */
#define INCLUDE_vTaskDelay                      1
#define INCLUDE_vTaskDelete                     1
#define INCLUDE_uxTaskGetStackHighWaterMark     1

/* ── 钩子 ──────────────────────────────────────────────────── */
#define configUSE_IDLE_HOOK                     1   /* IWDG feed */
#define configUSE_TICK_HOOK                     0
#define configUSE_MALLOC_FAILED_HOOK            0
#define configCHECK_FOR_STACK_OVERFLOW          1  /* 栈溢出检测 */

/* ── 软件定时器 (本阶段不使用) ────────────────────────────── */
#define configUSE_TIMERS                        0

/* ── 中断优先级映射 ────────────────────────────────────────── */
/**
 * STM32 使用 NVIC 优先级的高 4 bits (0-15)。
 * FreeRTOS 管理从中断安全调用 API 的优先级阈值。
 *
 * configKERNEL_INTERRUPT_PRIORITY:
 *   RTOS 内核临界区使用的优先级 (最低 = 15)。
 *   写入 BASEPRI 来屏蔽所有 ≤ 此值的中断。
 *   在 STM32 上: (15 << 4) = 240 = 0xF0
 *
 * configMAX_SYSCALL_INTERRUPT_PRIORITY:
 *   可以安全调用 FreeRTOS API 的最高逻辑优先级。
 *   优先级数值 > 此值的中断可以调用 FreeRTOS API。
 *   设为 5: 优先级 0-5 不能调用 API (更快, 用于关键中断)
 *           优先级 6-15 可以调用 API
 */
#define configKERNEL_INTERRUPT_PRIORITY         0xF0
#define configMAX_SYSCALL_INTERRUPT_PRIORITY    (5 << 4)
#define configLIBRARY_KERNEL_INTERRUPT_PRIORITY 15
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 5

/* ── 断言 ──────────────────────────────────────────────────── */
/* FreeRTOS internal assert — disabled for diagnostic */
#define configASSERT(cond)  ((void)0)

/* ── 钩子函数声明 ──────────────────────────────────────────── */
/* 如果启用 configUSE_MALLOC_FAILED_HOOK=1，需要实现 vApplicationMallocFailedHook */
/* 如果启用 configCHECK_FOR_STACK_OVERFLOW，需要实现 vApplicationStackOverflowHook */

#endif /* FREERTOS_CONFIG_H */
