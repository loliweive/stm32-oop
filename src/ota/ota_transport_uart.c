/**
 * @file    ota_transport_uart.c
 * @brief   UART 传输后端 — 最通用的 OTA 通信方式
 *
 * 使用项目的 UartPort (依赖注入模式) 作为物理层。
 * 通过 USB-TTL 模块或板载串口与上位机通信。
 *
 * 默认配置：USART1, 115200-8N1, PA9=TX, PA10=RX
 */
#include "ota_transport_uart.h"
#include "rcc.h"
#include "stm32f103xb.h"
#include <string.h>

/* ── vtable 实现 ──────────────────────────────────────────────── */
static void _uart_init(OtaTransport *self)
{
    UartXportCtx *ctx = (UartXportCtx *)self->ctx;

    /* 使能 GPIOA 和 USART1 时钟 */
    rcc_enable_gpio('A');
    rcc_enable_usart(1);

    /* PA9 = USART1 TX */
    GpioPin_ctor(&ctx->tx, GPIOA, GPIO_PIN_9);
    gpio_set_mode(&ctx->tx, GPIO_CNF_ALT_PP | GPIO_MODE_OUT_50MHZ);

    /* PA10 = USART1 RX */
    GpioPin_ctor(&ctx->rx, GPIOA, GPIO_PIN_10);
    gpio_set_mode(&ctx->rx, GPIO_CNF_FLOAT | GPIO_MODE_IN);

    /* 初始化 UART: 115200, 无注入 (使用真实硬件) */
    UartPort_ctor(&ctx->uart, USART1, 115200);
    uart_init(&ctx->uart);
}

static size_t _uart_send(OtaTransport *self, const uint8_t *data, size_t len)
{
    UartXportCtx *ctx = (UartXportCtx *)self->ctx;
    uart_send_buf(&ctx->uart, data, len);
    return len;
}

static size_t _uart_recv(OtaTransport *self, uint8_t *buf, size_t max_len, uint32_t timeout_ms)
{
    UartXportCtx *ctx = (UartXportCtx *)self->ctx;
    size_t received = 0;

    /* 简单轮询 — 对于 OTA 场景, 上位机会持续发送数据 */
    uint32_t elapsed = 0;
    while (received < max_len && elapsed < timeout_ms) {
        uint8_t byte;
        if (uart_recv(&ctx->uart, &byte)) {
            buf[received++] = byte;
            elapsed = 0; /* 收到数据, 重置超时 */
        } else {
            /* 简易延时 ~1ms (忙等待) */
            for (volatile int i = 0; i < 72000; i++) {}
            elapsed++;
        }
    }
    return received;
}

static void _uart_flush(OtaTransport *self)
{
    UartXportCtx *ctx = (UartXportCtx *)self->ctx;
    uint8_t dummy;
    /* 丢弃接收缓冲 */
    while (uart_recv(&ctx->uart, &dummy)) {}
}

/* ── vtable 实例 ──────────────────────────────────────────────── */
static const OtaTransportVtable uart_vtable = {
    .init  = _uart_init,
    .send  = _uart_send,
    .recv  = _uart_recv,
    .flush = _uart_flush,
};

/* ── 公开 API ──────────────────────────────────────────────────── */

/**
 * @brief 创建基于 UART 的 OTA 传输
 *
 * @param transport 未初始化的传输对象
 * @param ctx       传输上下文 (由调用者分配, 静态或全局)
 *
 * 使用示例:
 *   static UartXportCtx ctx;
 *   OtaTransport transport;
 *   ota_transport_uart_create(&transport, &ctx);
 *   ota_xport_init(&transport);
 */
void ota_transport_uart_create(OtaTransport *transport, UartXportCtx *ctx)
{
    memset(ctx, 0, sizeof(*ctx));
    transport->vtable = &uart_vtable;
    transport->ctx    = ctx;
}
