/**
 * @file    cmsis_os2.h
 * @brief   轻量 CMSIS-RTOS V2 API 子集 — 基于 FreeRTOS V11 内核
 *
 * 只实现项目实际使用的 API。完整版见 ARM CMSIS-FreeRTOS。
 * 目的: 应用代码使用 CMSIS-RTOS V2 标准 API, 底层 FreeRTOS 可替换。
 */
#ifndef CMSIS_OS2_H
#define CMSIS_OS2_H

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── 状态码 ─────────────────────────────────────────────── */
typedef enum {
    osOK           = 0,
    osError        = -1,
    osErrorTimeout = -2,
    osErrorResource= -3,
    osErrorParameter=-4,
} osStatus_t;

/* ── 线程 ────────────────────────────────────────────────── */
typedef TaskHandle_t osThreadId_t;

typedef struct {
    const char         *name;
    uint32_t            attr_bits;
    void               *cb_mem;
    uint32_t            cb_size;
    void               *stack_mem;
    uint32_t            stack_size;
    uint32_t            priority;
} osThreadAttr_t;

osThreadId_t osThreadNew(void (*func)(void *), void *arg, const osThreadAttr_t *attr);
osStatus_t   osThreadExit(void);
osStatus_t   osDelay(uint32_t ticks);

/* ── 内核信息 ────────────────────────────────────────────── */
uint32_t     osKernelGetTickCount(void);

/* ── 消息队列 (FreeRTOS queue 的包装) ─────────────────── */
typedef QueueHandle_t osMessageQueueId_t;

typedef struct {
    const char         *name;
    uint32_t            attr_bits;
    void               *cb_mem;
    uint32_t            cb_size;
    void               *mq_mem;
    uint32_t            mq_size;
} osMessageQueueAttr_t;

osMessageQueueId_t osMessageQueueNew(uint32_t msg_count, uint32_t msg_size,
                                      const osMessageQueueAttr_t *attr);
osStatus_t osMessageQueuePut(osMessageQueueId_t mq_id, const void *msg_ptr,
                              uint8_t msg_prio, uint32_t timeout);
osStatus_t osMessageQueueGet(osMessageQueueId_t mq_id, void *msg_ptr,
                              uint8_t *msg_prio, uint32_t timeout);

/* ── 定时器 (FreeRTOS timer 的包装) ───────────────────── */
typedef TimerHandle_t osTimerId_t;

typedef void (*osTimerFunc_t)(void *arg);

typedef enum {
    osTimerOnce   = 0,
    osTimerPeriodic = 1,
} osTimerType_t;

typedef struct {
    const char         *name;
    uint32_t            attr_bits;
    void               *cb_mem;
    uint32_t            cb_size;
} osTimerAttr_t;

osTimerId_t osTimerNew(osTimerFunc_t func, osTimerType_t type,
                        void *arg, const osTimerAttr_t *attr);
osStatus_t  osTimerStart(osTimerId_t timer_id, uint32_t ticks);
osStatus_t  osTimerStop(osTimerId_t timer_id);

/* ── 内核启动 ────────────────────────────────────────────── */
osStatus_t osKernelStart(void);

/* ── Tick 频率 (用于调试/计算) ─────────────────────────── */
#define osKernelGetTickFreq() ((uint32_t)configTICK_RATE_HZ)

#ifdef __cplusplus
}
#endif

#endif /* CMSIS_OS2_H */
