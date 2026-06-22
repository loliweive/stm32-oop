/**
 * @file    main.c
 * @brief   FreeRTOS CLI + 温湿度传感器 (DS18B20 / DHT11 可切换)
 *
 * ── 传感器切换 ─────────────────────────────────────────────
 *   修改下面的 SENSOR_TYPE 宏即可切换传感器:
 *     SENSOR_DS18B20  → M2  DS18B20 (OneWire, -55~125°C, 0.0625°C)
 *     SENSOR_DHT11    → M10 DHT11   (自有协议, 0~50°C + 湿度)
 *   两个模块共用 PA1, 可插拔替换, 编译时选择。
 */

#include "stm32f103xb.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include "rcc.h"
#include "gpio.h"
#include "uart.h"
#include "cli.h"
#include "ota_client.h"
#include "ota_transport_shared.h"
#include <stdio.h>
#include <string.h>

/* ═══════════════════════════════════════════════════════════════
 *  传感器选型 — 改这一行即可切换!
 * ═══════════════════════════════════════════════════════════════ */
#define SENSOR_DS18B20  1
#define SENSOR_DHT11    2
#define SENSOR_LIGHT    3
#define SENSOR_TYPE     SENSOR_DS18B20   /* <── 改这行切换传感器! */

#include "onewire.h"
#include "ds18b20.h"
#include "dht11.h"
#include "light_sensor.h"

/* ── 引脚 ────────────────────────────────────────────────── */
#define SENSOR_PORT   GPIOA
#define SENSOR_PIN    GPIO_PIN_1
#define UART_TX_PORT  GPIOA
#define UART_TX_PIN   GPIO_PIN_9
#define UART_RX_PORT  GPIOA
#define UART_RX_PIN   GPIO_PIN_10

/* ── 队列消息 ────────────────────────────────────────────── */
typedef struct {
    float    temp_c;
    uint8_t  humidity;        /* 255 = 不支持 */
    uint32_t timestamp_ms;
    bool     valid;
} TempReading;

/* ── 全局对象 ────────────────────────────────────────────── */
static QueueHandle_t temp_queue;
static Sensor       *sensor;          /* ← 多态指针! 不关心具体类型 */
static OneWireBus    ow_bus;          /* DS18B20 复用 */
static DS18B20       ds18b20_obj;     /* DS18B20 实例 */
static DHT11         dht11_obj;       /* DHT11 实例 */
static LightSensor   light_obj;       /* M7 光敏实例 */
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
    if (xQueueReceive(temp_queue, &r, 0) == pdTRUE) {
        if (r.valid) {
            if (r.humidity != 255) {
                cli_printf(c, "%s: %+5.1f C  %2u %%RH  (t=%lus)\r\n",
                           sensor_name(sensor), (double)r.temp_c,
                           (unsigned)r.humidity,
                           (unsigned long)(r.timestamp_ms / 1000));
            } else {
                cli_printf(c, "%s: %+5.1f C  (t=%lus)\r\n",
                           sensor_name(sensor), (double)r.temp_c,
                           (unsigned long)(r.timestamp_ms / 1000));
            }
        } else {
            cli_printf(c, "Sensor error — read failed\r\n");
        }
    } else {
        cli_printf(c, "No reading yet. Sensor warming up...\r\n");
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
    cli_printf(c, "  Sensor:    %s (PA1)\r\n", sensor_name(sensor));
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

/* ── OTA 状态 ──────────────────────────────────────────────────── */
static OtaTransport   ota_xport;
static SharedXportCtx ota_xport_ctx;
static OtaClient      ota_client;
static bool           ota_in_progress = false;

static void cmd_ota_start(CLI *c, int argc, char **argv)
{
    (void)argc; (void)argv;

    if (ota_in_progress) {
        cli_printf(c, "OTA already in progress\r\n");
        return;
    }

    cli_printf(c, "Starting OTA firmware update...\r\n");
    cli_printf(c, "CLI suspended — send firmware now.\r\n\r\n");

    ota_in_progress = true;

    /* 初始化 OTA (复用当前 UART) */
    ota_transport_shared_create(&ota_xport, &ota_xport_ctx);
    ota_xport_ctx.uart = &uart;
    ota_xport_init(&ota_xport);

    ota_client_init(&ota_client, &ota_xport);
    ota_client_start(&ota_client);

    /* 驱动 OTA 状态机 (CLI 在此时阻塞) */
    while (ota_client_poll(&ota_client)) {
        vTaskDelay(pdMS_TO_TICKS(10));  /* 让出 CPU */
    }

    if (ota_client_get_state(&ota_client) == OTA_STATE_COMPLETE) {
        cli_printf(c, "\r\nOTA complete! Booting new firmware...\r\n");
        ota_client_boot(&ota_client);
        /* 不应到达 — boot 导致软复位 */
    } else {
        cli_printf(c, "\r\nOTA failed (error=%d). CLI restored.\r\n",
                   ota_client.error_code);
    }

    ota_in_progress = false;
    cli_prompt(c);
}

static void cmd_ota_status(CLI *c, int argc, char **argv)
{
    (void)argc; (void)argv;

    if (!ota_in_progress) {
        cli_printf(c, "OTA: idle (use 'ota-start' to begin update)\r\n");
        return;
    }

    OtaClientState s = ota_client_get_state(&ota_client);
    const char *state_names[] = {
        "IDLE", "HANDSHAKE", "RECEIVING", "VERIFYING", "COMPLETE", "ERROR"
    };

    cli_printf(c, "OTA Status: %s\r\n", state_names[s]);
    if (s == OTA_STATE_RECEIVING) {
        uint8_t pct = ota_client_get_progress(&ota_client);
        cli_printf(c, "Progress: %u%%\r\n", (unsigned)pct);
    }
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
    { "ota-start",   cmd_ota_start,   "Start OTA firmware update" },
    { "ota-status",  cmd_ota_status,  "Show OTA update status" },
    { NULL, NULL, NULL }
};

/* ═══════════════════════════════════════════════════════════════
 *  Sensor Task — 独立温度采样
 * ═══════════════════════════════════════════════════════════════ */
static void task_sensor(void *params)
{
    (void)params;
    TempReading r;

    /* ── 传感器创建 — 唯一的编译时分支! ──────────────────── */
#if SENSOR_TYPE == SENSOR_DS18B20
    ow_init(&ow_bus, SENSOR_PORT, SENSOR_PIN);
    sensor = ds18b20_create(&ds18b20_obj, &ow_bus);
#elif SENSOR_TYPE == SENSOR_DHT11
    sensor = dht11_create(&dht11_obj, SENSOR_PORT, SENSOR_PIN);
    vTaskDelay(pdMS_TO_TICKS(1000));  /* DHT11 上电等待 1s */
#elif SENSOR_TYPE == SENSOR_LIGHT
    sensor = light_sensor_create(&light_obj,
                ADC1, 3,         /* PA3 = ADC1_IN3 模拟量 */
                GPIOB, GPIO_PIN_11);  /* PB11 = DO 数字阈值 */
    rcc_enable_gpio('B');        /* 使能 GPIOB 时钟 */
    rcc_enable_adc(1);           /* 使能 ADC1 时钟 */
#endif

    /* ── 检查传感器是否就绪 ──────────────────────────────── */
    if (!sensor || !sensor_is_present(sensor)) {
        memset(&r, 0, sizeof(r)); r.valid = false;
        r.timestamp_ms = xTaskGetTickCount();
        xQueueSend(temp_queue, &r, 0);
        vTaskDelete(NULL);
    }

    /* ── 采样循环 — 完全多态, 无传感器类型判断! ──────────── */
    while (1) {
        gpio_set(&led, 0);  /* LED 亮 — 采样中 */

        r.valid = sensor_read(sensor, &r.temp_c, &r.humidity);
        r.timestamp_ms = xTaskGetTickCount();

        xQueueOverwrite(temp_queue, &r);
        gpio_set(&led, 1);

        if (stream_mode && r.valid) {
            if (r.humidity != 255) {
                cli_printf(&cli, "[%6lus] %+5.1f C  %2u %%RH  (%s)\r\n",
                           (unsigned long)(r.timestamp_ms / 1000),
                           (double)r.temp_c, (unsigned)r.humidity,
                           sensor_name(sensor));
            } else {
                cli_printf(&cli, "[%6lus] %+5.1f C  (%s)\r\n",
                           (unsigned long)(r.timestamp_ms / 1000),
                           (double)r.temp_c, sensor_name(sensor));
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
