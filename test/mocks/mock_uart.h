/** Mock UART for host-side testing. */

#ifndef MOCK_UART_H
#define MOCK_UART_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "uart.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t  tx_buf[256];
    size_t   tx_count;
    uint8_t  rx_queue[256];
    size_t   rx_head;
    size_t   rx_tail;
    size_t   rx_count;
    bool     initialized;
} MockUartState;

void mock_uart_reset(MockUartState *s);
void mock_uart_inject(UartPort *uart, MockUartState *state);
void mock_uart_rx_push(MockUartState *s, uint8_t byte);

#ifdef __cplusplus
}
#endif

#endif /* MOCK_UART_H */
