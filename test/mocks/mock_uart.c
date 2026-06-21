/** Mock UART for host-side TDD. */
#include "mock_uart.h"
#include <string.h>

/* Forward the mock SerialInterface vtable */
static void _mock_write(uint8_t data);
static uint8_t _mock_read(void);
static uint8_t _mock_available(void);

static SerialInterface _mock_io = {
    .write     = _mock_write,
    .read      = _mock_read,
    .available = _mock_available,
};

static MockUartState *active_state = NULL;

static void _mock_write(uint8_t data)
{
    if (active_state && active_state->tx_count < sizeof(active_state->tx_buf)) {
        active_state->tx_buf[active_state->tx_count++] = data;
    }
}

static uint8_t _mock_read(void)
{
    if (active_state && active_state->rx_count > 0) {
        uint8_t b = active_state->rx_queue[active_state->rx_tail];
        active_state->rx_tail = (active_state->rx_tail + 1) % sizeof(active_state->rx_queue);
        active_state->rx_count--;
        return b;
    }
    return 0;
}

static uint8_t _mock_available(void)
{
    return active_state ? (uint8_t)active_state->rx_count : 0;
}

void mock_uart_reset(MockUartState *s)
{
    memset(s, 0, sizeof(*s));
}

void mock_uart_inject(UartPort *uart, MockUartState *state)
{
    uart->io = &_mock_io;
    active_state = state;
    state->initialized = true;
}

void mock_uart_rx_push(MockUartState *s, uint8_t byte)
{
    if (s->rx_count < sizeof(s->rx_queue)) {
        s->rx_queue[s->rx_head] = byte;
        s->rx_head = (s->rx_head + 1) % sizeof(s->rx_queue);
        s->rx_count++;
    }
}
