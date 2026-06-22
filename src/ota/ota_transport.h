/**
 * @file    ota_transport.h
 * @brief   OTA 传输层抽象接口 — 支持多种通信后端
 *
 * 定义了 OTA 通信所需的最小传输接口。
 * 任何实现了这 3 个函数的模块都可以作为 OTA 传输后端。
 *
 * 已实现的后端:
 *   - ota_transport_uart.c — 基于 UART 的传输 (最通用)
 *
 * 可扩展的后端:
 *   - ota_transport_spi.c  — 基于 SPI (如 ESP8266 AT 固件)
 *   - ota_transport_ble.c  — 基于 BLE (如 HM-10 模块)
 *   - ota_transport_can.c  — 基于 CAN 总线 (工业场景)
 *   - ota_transport_eth.c  — 基于以太网 (W5500 等)
 */

#ifndef OTA_TRANSPORT_H
#define OTA_TRANSPORT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 传输接口虚函数表
 *
 * 类似 HAL vtable 模式，允许运行时替换传输实现。
 */
typedef struct OtaTransport OtaTransport;

typedef struct {
    /** 初始化传输硬件 */
    void   (*init)(OtaTransport *self);

    /** 发送 len 字节, 阻塞直到完成 (或超时) */
    size_t (*send)(OtaTransport *self, const uint8_t *data, size_t len);

    /** 接收最多 max_len 字节, 返回实际接收数 (0 = 超时/无数据) */
    size_t (*recv)(OtaTransport *self, uint8_t *buf, size_t max_len, uint32_t timeout_ms);

    /** 清空接收缓冲区 */
    void   (*flush)(OtaTransport *self);
} OtaTransportVtable;

struct OtaTransport {
    const OtaTransportVtable *vtable;
    void *ctx;  /* 传输特定上下文 (如 UartPort*) */
};

/** 内联调度函数 */
static inline void   ota_xport_init(OtaTransport *t)        { t->vtable->init(t); }
static inline size_t ota_xport_send(OtaTransport *t, const uint8_t *d, size_t n) { return t->vtable->send(t, d, n); }
static inline size_t ota_xport_recv(OtaTransport *t, uint8_t *b, size_t n, uint32_t to) { return t->vtable->recv(t, b, n, to); }
static inline void   ota_xport_flush(OtaTransport *t)       { t->vtable->flush(t); }

#ifdef __cplusplus
}
#endif

#endif /* OTA_TRANSPORT_H */
