/**
 * @file    ota_transport_shared.c
 * @brief   共享 UART 传输实现 — 复用已有 UartPort
 */
#include "ota_transport_shared.h"
#include <string.h>

static void _init(OtaTransport *self)          { (void)self; /* UART 已初始化 */ }
static void _flush(OtaTransport *self)         { (void)self; /* no-op */ }

static size_t _send(OtaTransport *self, const uint8_t *data, size_t len)
{
    SharedXportCtx *c = (SharedXportCtx *)self->ctx;
    uart_send_buf(c->uart, data, len);
    return len;
}

static size_t _recv(OtaTransport *self, uint8_t *buf, size_t max_len, uint32_t timeout_ms)
{
    SharedXportCtx *c = (SharedXportCtx *)self->ctx;
    size_t n = 0;
    uint32_t elapsed = 0;

    while (n < max_len && elapsed < timeout_ms) {
        uint8_t byte;
        if (uart_recv(c->uart, &byte)) {
            if (byte == 0x1B) { c->cancelled = true; return 0; }  /* ESC → 取消 */
            buf[n++] = byte;
            elapsed = 0;
        } else {
            for (volatile int i = 0; i < 6000; i++) __asm__("nop");
            elapsed++;
        }
    }
    return n;
}

static const OtaTransportVtable _vtable = {
    .init = _init, .send = _send, .recv = _recv, .flush = _flush
};

void ota_transport_shared_create(OtaTransport *transport, SharedXportCtx *ctx)
{
    memset(transport, 0, sizeof(*transport));
    transport->vtable = &_vtable;
    transport->ctx    = ctx;
}
