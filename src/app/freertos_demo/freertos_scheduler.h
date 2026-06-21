/**
 * @file    freertos_scheduler.h
 * @brief   FreeRTOS scheduler adapter — implements SchedulerInterface
 *
 * 将 FreeRTOS API (xTaskCreate, vTaskStartScheduler, vTaskDelay)
 * 适配到我们定义的 SchedulerInterface 接口。
 *
 * 使用方式：
 *   1. 用 task_create() 创建任务
 *   2. 用 freertos_start() 启动调度器 (永不返回)
 *
 *   之后 FreeRTOS 接管 CPU，按优先级抢占调度所有任务。
 *
 * ── 与裸机调度器的对比 ──────────────────────────────────
 *
 *   裸机 (baremetal_scheduler):
 *     - 协作式轮询
 *     - 一个任务阻塞 = 整个系统停
 *     - 不需要栈分配
 *     - 适合简单状态机
 *
 *   FreeRTOS (freertos_scheduler):
 *     - 抢占式调度
 *     - 一个任务 delay → CPU 给其他任务
 *     - 每个任务需要独立栈
 *     - 适合复杂多任务系统
 */

#ifndef FREERTOS_SCHEDULER_H
#define FREERTOS_SCHEDULER_H

#include "FreeRTOS.h"
#include "task.h"

/* ── 任务描述符 (复用 plan 中定义的结构) ──────────────────── */

/**
 * @brief 任务参数
 *
 * 描述一个 FreeRTOS 任务的创建参数。
 * 所有字段在调用 task_create() 后不再被引用 (深拷贝到 TCB)。
 */
typedef struct {
    TaskFunction_t  func;        /**< 任务入口函数 */
    const char     *name;        /**< 任务名 (调试用, 最多 configMAX_TASK_NAME_LEN) */
    uint16_t        stack_words; /**< 栈大小 (words, 不是 bytes — STM32 word = 4 bytes) */
    void           *params;      /**< 传递给 func 的参数指针 */
    UBaseType_t     priority;    /**< 优先级 (0 = 最低, configMAX_PRIORITIES-1 = 最高) */
    TaskHandle_t   *handle;      /**< 输出: 任务句柄指针 (NULL = 不需要) */
} TaskParams;

/**
 * @brief 创建 FreeRTOS 任务
 *
 * 封装 xTaskCreate，提供更简洁的参数结构。
 *
 * @param p 任务参数 (栈、优先级等)
 * @return pdPASS 成功，pdFAIL 失败 (通常是堆不足)
 */
BaseType_t task_create(const TaskParams *p);

/**
 * @brief 启动 FreeRTOS 调度器
 *
 * 调用 vTaskStartScheduler()。
 * 此函数从不返回 (除非堆不足以创建 idle 任务)。
 * 返回时表示调度器启动失败。
 */
void freertos_start(void);

#endif /* FREERTOS_SCHEDULER_H */
