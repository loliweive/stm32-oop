/**
 * @file    main.c
 * @brief   FreeRTOS 多任务温度采集系统
 *
 * 架构:
 *   ┌──────────────┐     Queue      ┌──────────────┐
 *   │ Sensor Task  │─── temperature ─→│ Reporter Task │
 *   │ (prio 2)     │   data          │ (prio 1)     │
 *   │ 每 2s 采样   │                 │ UART 输出     │
 *   └──────────────┘                 └──────────────┘
 *
 * 演示 FreeRTOS 模式:
 *   - Queue: 任务间数据传递 (温度结构体)
 *   - vTaskDelay: 非阻塞周期采样
 *   - 独立栈空间: Sensor 512B, Reporter 1KB
 *
 * 硬件:
 *   PA1 = DS18B20 DQ (4.7kΩ 上拉到 3.3V)
 *   PA9 = USART1 TX, PA10 = USART1 RX
 *   PC13 = LED (采样指示)
 *
 * 构建:
 *   cmake -B build/freertos -DBUILD_MODE=freertos
 *   cmake --build build/freertos
 */

#include "stm32f103xb.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include "rcc.h"
#include "gpio.h"
#include "uart.h"
#include "onewire.h"
#include "ds18b20.h"

#include <stdio.h>

/* ── 引脚定义 ──────────────────────────────────────────────────── */
#define OW_PORT      GPIOA
#define OW_PIN       GPIO_PIN_1    /* PA1 = DS18B20 DQ */
#define UART_TX_PORT GPIOA
#define UART_TX_PIN  GPIO_PIN_9
#define UART_RX_PORT GPIOA
#define UART_RX_PIN  GPIO_PIN_10

/* ── 温度数据队列消息 ──────────────────────────────────────────── */
typedef struct {
    float    temp_c;
    uint32_t timestamp_ms;   /* 采样时刻 (xTaskGetTickCount) */
    bool     valid;           /* CRC 校验通过 */
} TempReading;

/* ── 队列句柄 ──────────────────────────────────────────────────── */
static QueueHandle_t temp_queue;

/* ── 外设 (任务间共享, 每个任务独占使用无需互斥锁) ───────────── */
static OneWireBus ow_bus;
static DS18B20    sensor;
static UartPort   uart;
static GpioPin    led;

/* ═══════════════════════════════════════════════════════════════
 *  Sensor Task — 周期采样, 推入队列
 * ═══════════════════════════════════════════════════════════════ */
static void task_sensor(void *params)
{
    (void)params;
    TempReading reading;

    /* 初始化 OneWire + DS18B20 */
    ow_init(&ow_bus, OW_PORT, OW_PIN);

    if (!ds18b20_init(&sensor, &ow_bus)) {
        /* 传感器未找到 — 发送错误到队列 */
        reading.valid        = false;
        reading.temp_c       = 0.0f;
        reading.timestamp_ms = xTaskGetTickCount();
        xQueueSend(temp_queue, &reading, 0);
        vTaskDelete(NULL);  /* 自我销毁 */
    }

    /* LED 初始化 — 采样指示 */
    GpioPin_ctor(&led, GPIOC, GPIO_PIN_13);
    gpio_set_mode(&led, GPIO_MODE_OUT_PP);
    gpio_set(&led, 1);  /* 初始关闭 */

    while (1) {
        /* LED 亮 — 正在采样 */
        gpio_set(&led, 0);

        /* 启动转换 + 等待完成 (DS18B20 12-bit 需要 ~750ms) */
        ds18b20_start_conversion(&sensor);
        ds18b20_wait_conversion(&sensor);

        /* 读暂存器并解析 */
        uint8_t sp[9];
        reading.valid = ds18b20_read_scratchpad(&sensor, sp);

        if (reading.valid) {
            reading.temp_c = ds18b20_parse_temp(sp);
        } else {
            reading.temp_c = 0.0f;
        }

        reading.timestamp_ms = xTaskGetTickCount();

        /* 推入队列 (非阻塞 — 队列满则丢弃旧数据) */
        xQueueOverwrite(temp_queue, &reading);

        /* LED 灭 — 采样完成 */
        gpio_set(&led, 1);

        /* 等待 2 秒再采样 (FreeRTOS 非阻塞延时) */
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

/* ═══════════════════════════════════════════════════════════════
 *  Reporter Task — 从队列取数据, 格式化输出到 UART
 * ═══════════════════════════════════════════════════════════════ */
static void task_reporter(void *params)
{
    (void)params;
    TempReading reading;
    char buf[100];
    int  len;

    /* 初始化 UART */
    {
        GpioPin tx, rx;
        GpioPin_ctor(&tx, UART_TX_PORT, UART_TX_PIN);
        gpio_set_mode(&tx, GPIO_CNF_ALT_PP | GPIO_MODE_OUT_50MHZ);
        GpioPin_ctor(&rx, UART_RX_PORT, UART_RX_PIN);
        gpio_set_mode(&rx, GPIO_CNF_FLOAT | GPIO_MODE_IN);
        UartPort_ctor(&uart, USART1, 115200);
        uart_init(&uart);
    }

    uart_send_str(&uart,
        "\r\n╔══════════════════════════════════╗\r\n"
        "║  FreeRTOS DS18B20 Thermometer   ║\r\n"
        "╚══════════════════════════════════╝\r\n\r\n");

    /* 打印表头 */
    uart_send_str(&uart, "  Time(s)   Temp(°C)   Status\r\n");
    uart_send_str(&uart, "  --------  ---------  ------\r\n");

    while (1) {
        /* 阻塞等待队列数据 (最多等 5 秒) */
        if (xQueueReceive(temp_queue, &reading, pdMS_TO_TICKS(5000)) == pdTRUE) {

            if (reading.valid) {
                len = snprintf(buf, sizeof(buf),
                               "  %8lu  %+8.2f   OK\r\n",
                               (unsigned long)(reading.timestamp_ms / 1000),
                               (double)reading.temp_c);
            } else {
                len = snprintf(buf, sizeof(buf),
                               "  %8lu  %8s   SENSOR ERROR\r\n",
                               (unsigned long)(reading.timestamp_ms / 1000),
                               "---.--");
            }
            uart_send_buf(&uart, (uint8_t *)buf, len);
        } else {
            /* 超时 — 传感器任务可能已退出 */
            uart_send_str(&uart, "  (timeout — sensor task may have stopped)\r\n");
        }
    }
}

/* ═══════════════════════════════════════════════════════════════
 *  main — 初始化硬件, 创建任务和队列, 启动调度器
 * ═══════════════════════════════════════════════════════════════ */
int main(void)
{
    /* 1. 时钟: 72MHz */
    rcc_set_sysclk(RCC_HSE, 9);
    rcc_enable_gpio('A');
    rcc_enable_gpio('C');
    rcc_enable_usart(1);

    /* 2. 创建温度数据队列 (深度=1, 只保留最新读数) */
    temp_queue = xQueueCreate(1, sizeof(TempReading));
    if (temp_queue == NULL) {
        /* 队列创建失败 — 堆不足, 增大 configTOTAL_HEAP_SIZE */
        while (1) { __asm__ volatile("nop"); }
    }

    /* 3. 创建 Sensor 任务 (优先级 2, 512 words = 2KB 栈) */
    xTaskCreate(
        task_sensor, "Sensor", 128, NULL, 2, NULL
    );

    /* 4. 创建 Reporter 任务 (优先级 1, 256 words = 1KB 栈) */
    xTaskCreate(
        task_reporter, "Reporter", 256, NULL, 1, NULL
    );

    /* 5. 启动调度器 — 永不返回 */
    vTaskStartScheduler();

    /* 不应到达 */
    while (1) {}
    return 0;
}
