/**
 * @file    ota_transport_shared.h
 * @brief   共享 UART 的 OTA 传输 — 复用已有 UartPort
 *
 * 用于 CLI 集成场景: CLI 和 OTA 共用同一个 UART。
 * OTA 期间 CLI 暂停, OTA 完成后恢复。
 *
 * 使用:
 *   OtaTransport t;
 *   ota_transport_shared_create(&t, &existing_uart);
 */

#ifndef OTA_TRANSPORT_SHARED_H
#define OTA_TRANSPORT_SHARED_H

#include "ota_transport.h"
#include "uart.h"

typedef struct {
    UartPort *uart;
} SharedXportCtx;

/** 创建共享 UART 传输 — 不创建新的 GPIO/UART, 复用已有 */
void ota_transport_shared_create(OtaTransport *transport, SharedXportCtx *ctx);

#endif
