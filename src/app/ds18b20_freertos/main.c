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
#include <stdlib.h>

/* ═══════════════════════════════════════════════════════════════
 *  传感器选型 — 改这一行即可切换!
 * ═══════════════════════════════════════════════════════════════ */
#define SENSOR_DS18B20  1
#define SENSOR_DHT11    2
#define SENSOR_LIGHT    3
#define SENSOR_BMP280   4
#define SENSOR_TYPE     SENSOR_DHT11   /* <── 改这行切换传感器! */

#include "onewire.h"
#include "ds18b20.h"
#include "dht11.h"
#include "light_sensor.h"
#include "bmp280.h"
#include "ssd1306.h"
#include "oled.h"
#include "i2c.h"
#include "spi_flash.h"
#include "iwdg.h"

/* ── OLED 引脚 ──────────────────────────────────────────── */
#define OLED_I2C       I2C1
#define OLED_ADDR      0x3C
#define OLED_SCL_PORT  GPIOB
#define OLED_SCL_PIN   GPIO_PIN_6
#define OLED_SDA_PORT  GPIOB
#define OLED_SDA_PIN   GPIO_PIN_7

/* ── 传感器引脚 ──────────────────────────────────────────── */
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
static Sensor       *light_sensor;     /* 光敏传感器 (常驻, 独立 IO) */
static OneWireBus    ow_bus;          /* DS18B20 复用 */
static DS18B20       ds18b20_obj;     /* DS18B20 实例 */
static DHT11         dht11_obj;       /* DHT11 实例 */
static LightSensor   light_obj;       /* M7 光敏实例 */
static BMP280        bmp280_obj;       /* BMP280 I2C 温压实例 */
static UartPort      uart;
static GpioPin       led;
static GpioPin       btn;            /* PB14 上拉按键 */
static CLI           cli;
static SSD1306       oled;
static SpiFlash      spiflash;
static bool          stream_mode = false;

/* ── UART 写回调 (给 CLI) ──────────────────────────────────────── */
static void uart_cli_write(const char *str, size_t len)
{
    uart_send_buf(&uart, (const uint8_t *)str, len);
}

/* ═══════════════════════════════════════════════════════════════
 *  CLI 命令实现
 * ═══════════════════════════════════════════════════════════════ */

/* ── FreeRTOS idle hook: 喂狗 ─────────────────────── */
void vApplicationIdleHook(void) { iwdg_feed(); }

static void cmd_help(CLI *c, int argc, char **argv)
{
    (void)argc; (void)argv;
    cli_show_help(c);
}

static void cmd_iwdg(CLI *c, int argc, char **argv)
{
    (void)argc; (void)argv;
    cli_printf(c, "IWDG test: stop feeding, expect reset in ~10s...\r\n");
    /* SysTick 精确计数 30s, IWDG ~10-26s 触发 (LSI 17-40kHz, RLR=4095) */
    uint32_t ms = 0;
    while (ms < 30000) {
        if (SysTick->CTRL & (1<<16)) ms++;
    }
    cli_printf(c, "IWDG did NOT reset — watchdog may be disabled\r\n");
}

static void cmd_temp(CLI *c, int argc, char **argv)
{
    (void)argc; (void)argv;

    TempReading r;
    if (xQueueReceive(temp_queue, &r, 0) == pdTRUE) {
        if (r.valid) {
            int ti = (int)r.temp_c, tf = (int)((r.temp_c-ti)*10);
            if (tf < 0) tf = -tf;
            if (r.humidity != 255) {
                cli_printf(c, "%s: %d.%d C  %u %%RH  (t=%lus)\r\n",
                           sensor_name(sensor), ti, tf,
                           (unsigned)r.humidity,
                           (unsigned long)(r.timestamp_ms / 1000));
            } else {
                cli_printf(c, "%s: %d.%d C  (t=%lus)\r\n",
                           sensor_name(sensor), ti, tf,
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
    cli_printf(c, "Stream started. Press ESC to stop.\r\n");
}

static void cmd_btn(CLI *c, int argc, char **argv)
{
    (void)argc; (void)argv;
    uint8_t state = gpio_get(&btn);
    cli_printf(c, "Button (PB14): %s (%s)\r\n",
               state == 0 ? "PRESSED" : "released",
               state == 0 ? "LOW — contact GND" : "HIGH — pull-up");
}

static void cmd_light(CLI *c, int argc, char **argv) {
    (void)argc; (void)argv;
    float lux; uint8_t dout;
    if (sensor_read(light_sensor, &lux, &dout)) {
        int pct = (int)lux;
        int frac = (int)((lux - pct) * 10);
        cli_printf(c, "Light(M7): %d.%d %%  DO=%u  (PA3+PB11)\r\n",
                   pct, frac, (unsigned)dout);
    } else {
        cli_printf(c, "Light sensor read failed\r\n");
    }
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
    RccConfig rcc; rcc_get_config(&rcc);

    cli_printf(c, "\r\n");
    cli_printf(c, "  MCU:       STM32F103C8T6 @ %luMHz\r\n",
               (unsigned long)(rcc.sysclk_hz / 1000000));
    cli_printf(c, "  Flash:     64KB  SRAM: 20KB\r\n");
    cli_printf(c, "  RTOS:      FreeRTOS V11, %luHz tick\r\n",
               (unsigned long)configTICK_RATE_HZ);
    cli_printf(c, "  Heap free: %lu bytes\r\n",
               (unsigned long)xPortGetFreeHeapSize());

    /* 外设状态 */
    cli_printf(c, "\r\n  Peripherals:\r\n");

    /* LED */
    cli_printf(c, "  [LED]      PC13 (active low) — 'led on|off|toggle'\r\n");

    /* Button */
    uint8_t btn_state = gpio_get(&btn);
    cli_printf(c, "  [BTN]      PB14 %s\r\n",
               btn_state == 0 ? "PRESSED" : "released");

    /* OLED */
    cli_printf(c, "  [OLED]     SSD1306 128x64, I2C1 PB6/PB7 @0x3C\r\n");

    /* Sensor */
    cli_printf(c, "  [SENSOR]   %s (PA1)  [LIGHT] M7 (PA3+PB11)\r\n", sensor_name(sensor));

    /* SPI Flash */
    const SpiFlashInfo *fi = spi_flash_get_info(&spiflash);
    if (fi->jedec_id != 0 && fi->jedec_id != 0xFFFFFF) {
        cli_printf(c, "  [FLASH]    %s, %luKB, JEDEC 0x%06lX, SPI1 PA5/6/7+PB9\r\n",
                   fi->name, (unsigned long)(fi->capacity / 1024),
                   (unsigned long)fi->jedec_id);
    } else {
        cli_printf(c, "  [FLASH]    NOT DETECTED (SPI1 PA5/6/7+PB9)\r\n");
    }

    /* OTA */
    cli_printf(c, "  [OTA]      ready — 'ota-start' to update\r\n");

    cli_printf(c, "\r\n");
}

static void cmd_uptime(CLI *c, int argc, char **argv)
{
    (void)argc; (void)argv;
    uint32_t s = (uint32_t)(xTaskGetTickCount() / configTICK_RATE_HZ);
    cli_printf(c, "Runtime: %lus (%lu:%02lu:%02lu)\r\n",
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

    ota_in_progress = true;

    /* 排空 UART RX */
    { uint8_t _dummy; while (uart_recv(&uart, &_dummy)) {} }

    /* 初始化 OTA */
    ota_transport_shared_create(&ota_xport, &ota_xport_ctx);
    ota_xport_ctx.uart = &uart;
    ota_xport_init(&ota_xport);
    ota_client_init(&ota_client, &ota_xport);
    ota_client.ext_flash = &spiflash;  /* 外置 Flash 做安全缓冲区 */
    ota_client_start(&ota_client);

    cli_printf(c, "Starting OTA firmware update...\r\n");
    cli_printf(c, "Send firmware now (60s timeout, ESC to cancel)...\r\n");
    cli_printf(c, "  60s remaining...\r\n");

    /* 倒计时: 前55s每5s提醒, 最后5s每秒提醒, ESC 取消 */
    uint32_t data_tick     = xTaskGetTickCount();
    uint32_t last_sec      = 0;
    int last_pct = -1;
    bool esc_pressed = false;

    while (ota_client_poll(&ota_client)) {
        /* ESC 取消 */
        if (ota_xport_ctx.cancelled) { esc_pressed = true; break; }
        if (ota_client_get_state(&ota_client) >= OTA_STATE_RECEIVING) {
            data_tick = xTaskGetTickCount();
            int pct = ota_client_get_progress(&ota_client);
            if (pct != last_pct) { cli_printf(c, "  %d%%\r\n", pct); last_pct = pct; }
        } else {
            uint32_t sec = (xTaskGetTickCount() - data_tick) / configTICK_RATE_HZ;
            uint32_t remaining = 60 - sec;
            if (sec > last_sec) {
                last_sec = sec;
                if (remaining <= 5 || remaining % 5 == 0)
                    cli_printf(c, "  %lus remaining...\r\n", (unsigned long)remaining);
            }
            if (sec >= 60) break;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    if (ota_client_get_state(&ota_client) != OTA_STATE_COMPLETE) {
        cli_printf(c, "\r\n%s\r\n", esc_pressed ? "OTA cancelled (ESC)." : "OTA timeout.");
        { uint8_t _dummy; while (uart_recv(&uart, &_dummy)) {} }
        ota_in_progress = false;
        ota_xport_ctx.cancelled = false;
        cli_prompt(c);
        return;
    }

    cli_printf(c, "\r\nOTA complete! Booting new firmware...\r\n");
    ota_client_boot(&ota_client);
    /* 不应到达 — boot 导致软复位 */
}

static void cmd_oled(CLI *c, int argc, char **argv)
{
    (void)argc; (void)argv;
    OledDisplay *d = &oled.base;
    oled_clear(d);
    oled_show_string(d, 0,  0, "STM32-oop", OLED_FONT_8X16);
    oled_show_string(d, 0, 16, "FreeRTOS+CLI", OLED_FONT_8X16);
    oled_show_string(d, 0, 32, sensor_name(sensor), OLED_FONT_6X8);
    const SpiFlashInfo *fi = spi_flash_get_info(&spiflash);
    char buf[32];
    int n = snprintf(buf, sizeof(buf), "%s %luKB", fi->name, (unsigned long)(fi->capacity/1024));
    oled_show_string(d, 0, 42, buf, OLED_FONT_6X8);
    oled_show_string(d, 0, 52, "Type help for CLI", OLED_FONT_6X8);
    oled_flush(d);
    cli_printf(c, "OLED updated.\r\n");
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

static void cmd_flash_id(CLI *c, int argc, char **argv)
{
    (void)argc; (void)argv;
    const SpiFlashInfo *info = spi_flash_get_info(&spiflash);
    cli_printf(c, "SPI Flash: %s\r\n", info->name);
    cli_printf(c, "  JEDEC ID: 0x%06lX\r\n", (unsigned long)info->jedec_id);
    cli_printf(c, "  Capacity: %lu KB (%lu MB)\r\n",
               (unsigned long)(info->capacity / 1024),
               (unsigned long)(info->capacity / 1048576));
    cli_printf(c, "  Page: %u B  Sector: %u KB  Block: %u KB\r\n",
               info->page_size, info->sector_size / 1024, info->block_size / 1024);
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
    { "btn",         cmd_btn,         "Read button state (PB14)" },
    { "flash-id",    cmd_flash_id,    "Read SPI Flash JEDEC ID" },
    { "info",        cmd_info,        "Show system information" },
    { "iwdg",        cmd_iwdg,        "Test watchdog (triggers reset)" },
    { "led",         cmd_led,         "Control LED: on|off|toggle" },
    { "light",       cmd_light,       "Read light sensor (PA3+PB11)" },
    { "oled",        cmd_oled,        "Show system info on OLED" },
    { "ota-start",   cmd_ota_start,   "Start OTA firmware update" },
    { "ota-status",  cmd_ota_status,  "Show OTA update status" },
    { "reset",       cmd_reset,       "Software reset MCU" },
    { "runtime",     cmd_uptime,      "Show system runtime" },
    { "temp",        cmd_temp,        "Read temperature once" },
    { "temp-stream", cmd_temp_stream, "Stream temp (ESC to stop)" },
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
#elif SENSOR_TYPE == SENSOR_BMP280
    /* BMP280: I2C1 PB6/PB7, 与 OLED 共用 (I2C 时钟已在 OLED 初始化中使能) */
    sensor = bmp280_create(&bmp280_obj, I2C1, 0x76);
#endif

    /* 光敏传感器常驻 — PA3(ADC)+PB11(GPIO), 与 PA1 温度传感器无冲突 */
    rcc_enable_gpio('B'); rcc_enable_adc(1);
    light_sensor = light_sensor_create(&light_obj, ADC1, 3, GPIOB, GPIO_PIN_11);

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

        /* 刷新 OLED 显示 */
        if (r.valid) {
            char l1[22], l2[22];
            int ti = (int)r.temp_c, tf = (int)((r.temp_c-ti)*10);
            if (tf < 0) tf = -tf;
            snprintf(l1, sizeof(l1), "%d.%d C", ti, tf);
            snprintf(l2, sizeof(l2), "%s", sensor_name(sensor));
            if (r.humidity != 255) {
                snprintf(l1, sizeof(l1), "%d.%d C  %u%%RH",
                         ti, tf, (unsigned)r.humidity);
            }
            {
                OledDisplay *d = &oled.base;
                oled_clear(d);
                oled_show_string(d, 0,  0, "STM32 Sensor", OLED_FONT_8X16);
                oled_show_string(d, 0, 16, l1, OLED_FONT_8X16);
                oled_show_string(d, 0, 32, l2, OLED_FONT_6X8);
                oled_flush(d);
            }
        }

        gpio_set(&led, 1);

        /* stream output handled by CLI task (queue-based, no reentrancy risk) */
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
        /* 排空 UART RX — 避免 ESC 序列丢字节 */
        for (int timeout = 2000; timeout > 0; timeout--) {
            if (uart_recv(&uart, &byte)) {
                if (byte == 0x1B && stream_mode) {  /* ESC 退出流模式 */
                    stream_mode = false;
                    cli_printf(&cli, "\r\nStream stopped (ESC).\r\n");
                    cli_prompt(&cli);
                    continue;
                }
                cli_feed(&cli, (char)byte);
                timeout = 2000;
            }
            __asm__("nop");
        }
        vTaskDelay(pdMS_TO_TICKS(5));
        iwdg_feed();

        /* temp-stream: 从队列读并输出 (CLI context, 无重入风险) */
        if (stream_mode) {
            TempReading sr;
            if (xQueueReceive(temp_queue, &sr, 0) == pdTRUE && sr.valid) {
                int ti=(int)sr.temp_c, tf=(int)((sr.temp_c-ti)*10);
                if(tf<0)tf=-tf;
                if (sr.humidity!=255)
                    cli_printf(&cli, "  %d.%dC  %d%%  %lus\r\n",
                        ti, tf, (unsigned)sr.humidity,
                        (unsigned long)(sr.timestamp_ms/1000));
                else
                    cli_printf(&cli, "  %d.%dC  %lus\r\n",
                        ti, tf, (unsigned long)(sr.timestamp_ms/1000));
            }
        }
    }
}

/* ═══════════════════════════════════════════════════════════════
 *  main — 初始化, 创建任务, 启动调度器
 * ═══════════════════════════════════════════════════════════════ */

/* Raw UART for early boot diagnostics */
static void _dputc(char c) { while(!(USART1->SR&(1<<7))){} USART1->DR=c; }
static void _dputs(const char *s) { while(*s) _dputc(*s++); }
#define DLOG(msg) do { _dputs("\r\n" msg); } while(0)

int main(void)
{
    rcc_set_sysclk(RCC_PLL, 9);
    rcc_enable_gpio('A'); rcc_enable_gpio('C'); rcc_enable_usart(1);

    /* Raw UART init for diagnostics */
    GPIOA->CRH = (GPIOA->CRH & ~(0xF<<4)) | (0xB<<4); /* PA9 TX */
    USART1->BRR = 625; USART1->CR1 = (1<<13)|(1<<3);
    DLOG("CP0: boot");

    /* 初始化 LED (PC13) */
    GpioPin_ctor(&led, GPIOC, GPIO_PIN_13);
    gpio_set_mode(&led, GPIO_MODE_OUT_PP);
    gpio_set(&led, 1);
    DLOG("CP1: LED ok");

    /* 初始化按键 (PB14, 上拉 — 按下=低电平) */
    rcc_enable_gpio('B');
    GpioPin_ctor(&btn, GPIOB, GPIO_PIN_14);
    gpio_set_mode(&btn, 0x8 | GPIO_MODE_IN);  /* CNF=10: input with pull-up */
    gpio_set(&btn, 1);
    DLOG("CP2: BTN ok");

    /* 初始化 OLED (PB6=SCL, PB7=SDA, I2C1) */
    rcc_enable_i2c(1);
    {
        GpioPin scl, sda;
        GpioPin_ctor(&scl, OLED_SCL_PORT, OLED_SCL_PIN);
        gpio_set_mode(&scl, GPIO_CNF_ALT_OD | GPIO_MODE_OUT_50MHZ);
        GpioPin_ctor(&sda, OLED_SDA_PORT, OLED_SDA_PIN);
        gpio_set_mode(&sda, GPIO_CNF_ALT_OD | GPIO_MODE_OUT_50MHZ);

        I2cPort i2c_port;
        I2cPort_ctor(&i2c_port, OLED_I2C, 400000, 36000000);  /* 400kHz @ 8MHz HSI */
        i2c_init(&i2c_port);
    }
    ssd1306_ctor(&oled, OLED_I2C, OLED_ADDR);
    {
        OledDisplay *d = &oled.base;
        oled_init(d);
        oled_clear(d);
        oled_show_string(d, 0,  0, "STM32-oop", OLED_FONT_8X16);
        oled_show_string(d, 0, 16, "OLED Ready", OLED_FONT_8X16);
        oled_show_string(d, 0, 32, "PB6/PB7 I2C", OLED_FONT_6X8);
        oled_flush(d);
    }
    DLOG("CP3: OLED ok");

    /* 初始化 SPI Flash */
#if 1
    rcc_enable_spi(1);
    {
        GpioPin sck, miso, mosi;
        GpioPin_ctor(&sck,  GPIOA, GPIO_PIN_5);
        gpio_set_mode(&sck,  GPIO_CNF_ALT_PP | GPIO_MODE_OUT_50MHZ);
        GpioPin_ctor(&miso, GPIOA, GPIO_PIN_6);
        gpio_set_mode(&miso, GPIO_CNF_FLOAT | GPIO_MODE_IN);
        GpioPin_ctor(&mosi, GPIOA, GPIO_PIN_7);
        gpio_set_mode(&mosi, GPIO_CNF_ALT_PP | GPIO_MODE_OUT_50MHZ);
    }
    spi_flash_init(&spiflash, SPI1, GPIOB, GPIO_PIN_9);
    DLOG("CP4: SPI ok");
#endif

    /* 创建温度数据队列 */
    temp_queue = xQueueCreate(1, sizeof(TempReading));
    configASSERT(temp_queue != NULL);
    DLOG("CP5: Q ok");

    /* 创建任务 */
    xTaskCreate(task_sensor,  "Sensor", 128, NULL, 1, NULL);
    xTaskCreate(task_cli,     "CLI",    512, NULL, 2, NULL);
    DLOG("CP6: tasks ok");

    /* IWDG: 必须在调度器启动前最后一刻初始化, 否则提前超时复位 */
    iwdg_init(6, 0xFFF);  /* PR=6 /256, RLR=4095: ~10-26s (LSI 17-40kHz). 防误复位 */
    DLOG("CP7: IWDG ok");

    vTaskStartScheduler();
    DLOG("CP7: NEVER");  /* should never reach */
    while (1) {}
    return 0;
}
