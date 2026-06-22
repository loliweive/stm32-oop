/**
 * @file    main.c
 * @brief   OTA Demo — 应用侧 OTA 接收示例
 *
 * 演示如何在应用中嵌入 OTA 客户端:
 *   1. 正常运行时执行应用逻辑 (LED 闪烁)
 *   2. 检测到 OTA 命令后启动更新流程
 *   3. 更新完成后自动复位, bootloader 加载新固件
 *
 * 构建 (应用固件, 从 0x08002000 开始):
 *   需要单独的应用链接脚本 (Flash origin = 0x08002000)
 *
 * 上位机工具:
 *   配合 tools/ota_sender.py 发送固件
 */

#include "stm32f103xb.h"
#include "ota_config.h"
#include "ota_transport.h"
#include "ota_transport_uart.h"
#include "ota_client.h"
#include "gpio.h"
#include "rcc.h"
#include <string.h>

static UartXportCtx  xport_ctx;
static OtaTransport  transport;
static OtaClient     ota;
static GpioPin       led;
static volatile int  g_ota_requested = 0;

/* ── LED ──────────────────────────────────────────────────────── */
static void led_setup(void)
{
    rcc_enable_gpio('C');
    GpioPin_ctor(&led, GPIOC, GPIO_PIN_13);
    gpio_set_mode(&led, GPIO_MODE_OUT_PP);
}

/* ── OTA 进度回调 ─────────────────────────────────────────────── */
static void ota_progress(uint32_t received, uint32_t total)
{
    /* 进度可通过 UART 发出, 此处简化: LED 快速闪烁 */
    static int t = 0;
    t = !t;
    if (t) gpio_set(&led, 0); else gpio_set(&led, 1);
    (void)received; (void)total;
}

static void ota_complete(void)
{
    /* 更新完成 — 快速闪烁 3 次 */
    for (int i = 0; i < 3; i++) {
        gpio_set(&led, 0); for (volatile int d = 0; d < 500000; d++) {}
        gpio_set(&led, 1); for (volatile int d = 0; d < 500000; d++) {}
    }
}

/* ── 主入口 ───────────────────────────────────────────────────── */
int main(void)
{
    rcc_set_sysclk(RCC_HSE, 9);  /* 72MHz */
    led_setup();

    /* 初始化 OTA 传输 */
    ota_transport_uart_create(&transport, &xport_ctx);
    ota_xport_init(&transport);

    /* 初始化 OTA 客户端 */
    ota_client_init(&ota, &transport);
    ota.on_progress = ota_progress;
    ota.on_complete = ota_complete;

    /* ── 主循环 ────────────────────────────────────────────── */
    while (1) {
        /* 应用逻辑: LED 慢闪 (正常运行指示) */
        gpio_toggle(&led);
        for (volatile int i = 0; i < 5000000; i++) {}

        /* 检查是否有 OTA 请求 (通过 UART 接收 HELLO 帧检测) */
        uint8_t buf[OTA_FRAME_MAX_SIZE];
        size_t n = ota_xport_recv(&transport, buf, sizeof(buf), 10);
        if (n > 0) {
            uint8_t type, seq;
            const uint8_t *payload;
            size_t plen;
            if (ota_frame_decode(buf, n, &type, &seq, &payload, &plen)
                && type == OTA_CMD_HELLO) {

                /* 检测到 OTA 请求 — 进入更新流程 */
                ota_client_start(&ota);

                /* 驱动 OTA 状态机直到完成 */
                while (ota_client_poll(&ota)) {}

                if (ota_client_get_state(&ota) == OTA_STATE_COMPLETE) {
                    ota_client_boot(&ota); /* 复位 → bootloader → 新固件 */
                }
            }
        }
    }

    return 0;
}
