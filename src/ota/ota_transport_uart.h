#ifndef OTA_TRANSPORT_UART_H
#define OTA_TRANSPORT_UART_H

#include "ota_transport.h"
#include "uart.h"
#include "gpio.h"

/* ── UART 传输上下文 (暴露给调用者用于静态分配) ──────────────── */
typedef struct {
    UartPort uart;
    GpioPin  tx;
    GpioPin  rx;
} UartXportCtx;

/**
 * @brief 创建 UART 传输
 * @param transport 输出: 初始化后的传输对象
 * @param ctx       用户分配的上下文内存
 */
void ota_transport_uart_create(OtaTransport *transport, UartXportCtx *ctx);

#endif
