/**
 * @file    cmsis_os2.c
 * @brief   轻量 CMSIS-RTOS V2 → FreeRTOS V11 适配层
 *
 * 为项目实际使用的 CMSIS-RTOS V2 API 子集提供 FreeRTOS 实现。
 * 后续换 RTOS 只需替换此文件。
 */
#include "cmsis_os2.h"

/* ── 线程 ────────────────────────────────────────────────── */

osThreadId_t osThreadNew(void (*func)(void *), void *arg,
                          const osThreadAttr_t *attr)
{
    const char *name = attr ? attr->name : "thread";
    uint16_t stack   = (uint16_t)(attr ? attr->stack_size : 128);
    UBaseType_t prio = (UBaseType_t)(attr ? attr->priority : 1);
    TaskHandle_t handle = NULL;

    BaseType_t ret = xTaskCreate((TaskFunction_t)func, name,
                                  stack, arg, prio, &handle);
    return (ret == pdPASS) ? handle : NULL;
}

osStatus_t osThreadExit(void)
{
    vTaskDelete(NULL);
    return osOK;  /* unreachable */
}

osStatus_t osDelay(uint32_t ticks)
{
    vTaskDelay((TickType_t)ticks);
    return osOK;
}

/* ── 内核信息 ────────────────────────────────────────────── */

uint32_t osKernelGetTickCount(void)
{
    return (uint32_t)xTaskGetTickCount();
}

/* ── 消息队列 ────────────────────────────────────────────── */

osMessageQueueId_t osMessageQueueNew(uint32_t msg_count, uint32_t msg_size,
                                      const osMessageQueueAttr_t *attr)
{
    (void)attr;
    return xQueueCreate((UBaseType_t)msg_count, (UBaseType_t)msg_size);
}

osStatus_t osMessageQueuePut(osMessageQueueId_t mq_id, const void *msg_ptr,
                              uint8_t msg_prio, uint32_t timeout)
{
    (void)msg_prio;
    BaseType_t ret;
    if (mq_id == NULL) return osErrorParameter;
    /* Length-1 queue overwrite: use xQueueOverwrite */
    if (uxQueueSpacesAvailable(mq_id) == 0 && uxQueueMessagesWaiting(mq_id) == 1) {
        (void)xQueueReceive(mq_id, (void *)msg_ptr, 0); /* drain old item */
    }
    ret = xQueueSendToBack(mq_id, msg_ptr, (TickType_t)timeout);
    return (ret == pdPASS) ? osOK : osError;
}

osStatus_t osMessageQueueGet(osMessageQueueId_t mq_id, void *msg_ptr,
                              uint8_t *msg_prio, uint32_t timeout)
{
    (void)msg_prio;
    if (mq_id == NULL) return osErrorParameter;
    BaseType_t ret = xQueueReceive(mq_id, msg_ptr, (TickType_t)timeout);
    return (ret == pdPASS) ? osOK : osErrorTimeout;
}

/* ── 定时器 ──────────────────────────────────────────────── */

static void _timer_callback_wrapper(TimerHandle_t xTimer)
{
    osTimerFunc_t fn = (osTimerFunc_t)pvTimerGetTimerID(xTimer);
    if (fn) fn(xTimer);
}

osTimerId_t osTimerNew(osTimerFunc_t func, osTimerType_t type,
                        void *arg, const osTimerAttr_t *attr)
{
    (void)arg;
    const char *name = attr ? attr->name : "timer";
    UBaseType_t reload = (type == osTimerPeriodic) ? pdTRUE : pdFALSE;
    return xTimerCreate(name, (TickType_t)1, reload, (void *)func,
                         _timer_callback_wrapper);
}

osStatus_t osTimerStart(osTimerId_t timer_id, uint32_t ticks)
{
    if (timer_id == NULL) return osErrorParameter;
    TickType_t period = (ticks > 0) ? (TickType_t)ticks : 1;
    (void)xTimerChangePeriod(timer_id, period, 0);
    BaseType_t ret = xTimerStart(timer_id, 0);
    return (ret == pdPASS) ? osOK : osError;
}

osStatus_t osTimerStop(osTimerId_t timer_id)
{
    if (timer_id == NULL) return osErrorParameter;
    BaseType_t ret = xTimerStop(timer_id, 0);
    return (ret == pdPASS) ? osOK : osError;
}

/* ── 内核启动 ────────────────────────────────────────────── */

osStatus_t osKernelStart(void)
{
    vTaskStartScheduler();
    return osOK;  /* unreachable if scheduler started */
}
