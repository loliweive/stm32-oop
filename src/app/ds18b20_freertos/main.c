/**
 * @file    main.c
 * @brief   FreeRTOS CLI + DS18B20 温度采集系统
 *
 * 任务架构:
 *   ┌────────────────┐   Queue    ┌─────────────────┐
 *   │  Sensor Task   │── temp ──→│   CLI Task       │
 *   │  prio 2, 512B  │            │   prio 1, 1KB    │
 *   │  周期采样 2s    │            │   串口交互式命令行 │
 *   └────────────────┘            └─────────────────┘
 *
 * CLI 命令:
 *   help         显示所有命令
 *   temp         读取一次温度
 *   temp-stream  开始连续温度输出
 *   temp-stop    停止连续温度输出
 *   led on|off|toggle  控制板载 LED
 *   info         显示系统信息
 *   uptime       显示运行时间
 *   reset        软复位 MCU
 *
 * 硬件:
 *   PA1 = DS18B20 DQ (4.7kΩ 上拉到 3.3V)
 *   PA9 = USART1 TX, PA10 = USART1 RX
 *   PC13 = LED
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
#include "cli.h"

#include <stdio.h>
#include <string.h>

/* ── 引脚 ──────────────────────────────────────────────────────── */
#define OW_PORT      GPIOA
#define OW_PIN       GPIO_PIN_1
#define UART_TX_PORT GPIOA
#define UART_TX_PIN  GPIO_PIN_9
#define UART_RX_PORT GPIOA
#define UART_RX_PIN  GPIO_PIN_10

/* ── 温度数据 ──────────────────────────────────────────────────── */
typedef struct {
    float    temp_c;
    uint32_t timestamp_ms;
    bool     valid;
} TempReading;

/* ── 全局对象 ──────────────────────────────────────────────────── */
static QueueHandle_t temp_queue;
static OneWireBus    ow_bus;
static DS18B20       sensor;
static UartPort      uart;
static GpioPin       led;
static CLI           cli;
static bool          stream_mode = false;

/* ── UART 写回调 (给 CLI) ──────────────────────────────────────── */
static void uart_cli_write(const char *str, size_t len)
{
    uart_send_buf(&uart, (const uint8_t *)str, len);
}

/* ═══════════════════════════════════════════════════════════════
 *  CLI 命令实现
 * ═══════════════════════════════════════════════════════════════ */

static void cmd_help(CLI *c, int argc, char **argv)
{
    (void)argc; (void)argv;
    cli_show_help(c);
}

static void cmd_temp(CLI *c, int argc, char **argv)
{
    (void)argc; (void)argv;

    TempReading r;
    /* 非阻塞尝试从队列获取最新读数 */
    if (xQueueReceive(temp_queue, &r, 0) == pdTRUE) {
        if (r.valid) {
            cli_printf(c, "Temperature: %+6.2f C  (t=%lus)\r\n",
                       (double)r.temp_c, (unsigned long)(r.timestamp_ms / 1000));
        } else {
            cli_printf(c, "Sensor error — CRC check failed\r\n");
        }
    } else {
        cli_printf(c, "No reading available yet. Wait for sensor...\r\n");
    }
}

static void cmd_temp_stream(CLI *c, int argc, char **argv)
{
    (void)c; (void)argc; (void)argv;
    stream_mode = true;
    cli_printf(c, "Continuous temperature output started.\r\n");
    cli_printf(c, "Type 'temp-stop' to stop.\r\n");
}

static void cmd_temp_stop(CLI *c, int argc, char **argv)
{
    (void)c; (void)argc; (void)argv;
    stream_mode = false;
    cli_printf(c, "Temperature stream stopped.\r\n");
}

static void cmd_led(CLI *c, int argc, char **argv)
{
    if (argc < 2) {
        cli_printf(c, "Usage: led on|off|toggle\r\n");
        return;
    }
    if (strcmp(argv[1], "on") == 0) {
        gpio_set(&led, 0); /* PC13 低电平点亮 */
        cli_printf(c, "LED ON\r\n");
    } else if (strcmp(argv[1], "off") == 0) {
        gpio_set(&led, 1);
        cli_printf(c, "LED OFF\r\n");
    } else if (strcmp(argv[1], "toggle") == 0) {
        gpio_toggle(&led);
        cli_printf(c, "LED toggled\r\n");
    } else {
        cli_printf(c, "Unknown: '%s'. Use on|off|toggle\r\n", argv[1]);
    }
}

static void cmd_info(CLI *c, int argc, char **argv)
{
    (void)argc; (void)argv;
    cli_printf(c, "\r\n");
    cli_printf(c, "  System:    STM32F103C8T6 (Blue Pill)\r\n");
    cli_printf(c, "  Core:      Cortex-M3 @ 72MHz\r\n");
    cli_printf(c, "  Flash:     64KB\r\n");
    cli_printf(c, "  SRAM:      20KB\r\n");
    cli_printf(c, "  RTOS:      FreeRTOS V11\r\n");
    cli_printf(c, "  Tick:      %lu Hz\r\n", (unsigned long)configTICK_RATE_HZ);
    cli_printf(c, "  Heap free: %lu bytes\r\n", (unsigned long)xPortGetFreeHeapSize());
    cli_printf(c, "  Sensors:   DS18B20 (PA1, OneWire)\r\n");
    cli_printf(c, "\r\n");
}

static void cmd_uptime(CLI *c, int argc, char **argv)
{
    (void)argc; (void)argv;
    uint32_t s = (uint32_t)(xTaskGetTickCount() / configTICK_RATE_HZ);
    cli_printf(c, "Uptime: %lus (%lu:%02lu:%02lu)\r\n",
               (unsigned long)s,
               (unsigned long)(s / 3600),
               (unsigned long)((s % 3600) / 60),
               (unsigned long)(s % 60));
}

static void cmd_reset(CLI *c, int argc, char **argv)
{
    (void)c; (void)argc; (void)argv;
    cli_printf(c, "Resetting...\r\n");
    vTaskDelay(pdMS_TO_TICKS(100));

    /* SCB AIRCR 软复位 */
    #define AIRCR_ADDR         ((volatile uint32_t *)0xE000ED0CUL)
    #define AIRCR_VECTKEY      (0x05FAUL << 16)
    #define AIRCR_SYSRESETREQ  (1UL << 2)
    *AIRCR_ADDR = AIRCR_VECTKEY | AIRCR_SYSRESETREQ;
    while (1) {}
}

/* ── 命令表 (NULL 终止) ──────────────────────────────────────── */
static const CLICommand commands[] = {
    { "help",        cmd_help,        "Show this help" },
    { "temp",        cmd_temp,        "Read temperature once" },
    { "temp-stream", cmd_temp_stream, "Start continuous temp output" },
    { "temp-stop",   cmd_temp_stop,   "Stop continuous temp output" },
    { "led",         cmd_led,         "Control LED: on|off|toggle" },
    { "info",        cmd_info,        "Show system information" },
    { "uptime",      cmd_uptime,      "Show system uptime" },
    { "reset",       cmd_reset,       "Software reset MCU" },
    { NULL, NULL, NULL }
};

/* ═══════════════════════════════════════════════════════════════
 *  Sensor Task — 独立温度采样
 * ═══════════════════════════════════════════════════════════════ */
static void task_sensor(void *params)
{
    (void)params;
    TempReading r;

    ow_init(&ow_bus, OW_PORT, OW_PIN);
    if (!ds18b20_init(&sensor, &ow_bus)) {
        r.valid = false; r.temp_c = 0.0f;
        r.timestamp_ms = xTaskGetTickCount();
        xQueueSend(temp_queue, &r, 0);
        vTaskDelete(NULL);
    }

    while (1) {
        /* LED 指示采样 */
        gpio_set(&led, 0);

        ds18b20_start_conversion(&sensor);
        ds18b20_wait_conversion(&sensor);

        uint8_t sp[9];
        r.valid = ds18b20_read_scratchpad(&sensor, sp);
        r.temp_c = r.valid ? ds18b20_parse_temp(sp) : 0.0f;
        r.timestamp_ms = xTaskGetTickCount();

        xQueueOverwrite(temp_queue, &r);

        gpio_set(&led, 1);

        /* 流模式: 立即推送给 CLI */
        if (stream_mode) {
            if (r.valid) {
                cli_printf(&cli, "[%6lus] %+6.2f C\r\n",
                           (unsigned long)(r.timestamp_ms / 1000),
                           (double)r.temp_c);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

/* ═══════════════════════════════════════════════════════════════
 *  CLI Task — 交互式命令行
 * ═══════════════════════════════════════════════════════════════ */
static void task_cli(void *params)
{
    (void)params;

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

    /* 初始化 CLI */
    cli_init(&cli, commands, uart_cli_write, NULL);

    /* 欢迎信息 */
    cli_printf(&cli, "\r\n");
    cli_printf(&cli, "╔══════════════════════════════════════╗\r\n");
    cli_printf(&cli, "║  STM32-oop  Interactive Shell        ║\r\n");
    cli_printf(&cli, "║  FreeRTOS + DS18B20 + CLI            ║\r\n");
    cli_printf(&cli, "╚══════════════════════════════════════╝\r\n");
    cli_printf(&cli, "Type 'help' for available commands.\r\n\r\n");
    cli_prompt(&cli);

    while (1) {
        uint8_t byte;
        if (uart_recv(&uart, &byte)) {
            cli_feed(&cli, (char)byte);
        }
        vTaskDelay(pdMS_TO_TICKS(10));  /* 10ms 轮询 */
    }
}

/* ═══════════════════════════════════════════════════════════════
 *  main — 初始化, 创建任务, 启动调度器
 * ═══════════════════════════════════════════════════════════════ */
int main(void)
{
    rcc_set_sysclk(RCC_HSE, 9);
    rcc_enable_gpio('A');
    rcc_enable_gpio('C');
    rcc_enable_usart(1);

    /* 初始化 LED (PC13) */
    GpioPin_ctor(&led, GPIOC, GPIO_PIN_13);
    gpio_set_mode(&led, GPIO_MODE_OUT_PP);
    gpio_set(&led, 1);

    /* 创建温度数据队列 */
    temp_queue = xQueueCreate(1, sizeof(TempReading));
    configASSERT(temp_queue != NULL);

    /* 创建任务 */
    xTaskCreate(task_sensor,  "Sensor", 128, NULL, 2, NULL);
    xTaskCreate(task_cli,     "CLI",    512, NULL, 1, NULL);

    vTaskStartScheduler();
    while (1) {}
    return 0;
}
