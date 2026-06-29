/**
 * @file    uart.c
 * @brief   UART — DI pattern + HAL API for TX/RX, manual register init
 */
#include "uart.h"
#include "stm32f1xx_hal.h"
#include "rcc.h"

void UartPort_ctor(UartPort *self, void *usart, uint32_t baud)
{
    self->usart    = usart;
    self->io       = NULL;
    self->baudrate = baud;
    rb_init(&self->rx_buf, self->rx_storage, sizeof(self->rx_storage));
    rb_init(&self->tx_buf, self->tx_storage, sizeof(self->tx_storage));
}

void uart_init(UartPort *self)
{
    if (self->io) return;
    if (self->baudrate == 0) return;

    USART_TypeDef *u = (USART_TypeDef *)self->usart;
    RccConfig rcc_cfg;
    rcc_get_config(&rcc_cfg);
    uint32_t pclk = (u == USART1) ? rcc_cfg.pclk2_hz : rcc_cfg.pclk1_hz;
    u->BRR = (pclk + self->baudrate / 2) / self->baudrate;
    u->CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;
}

static UART_HandleTypeDef _make_huart(UartPort *self)
{
    UART_HandleTypeDef h = {0};
    h.Instance = (USART_TypeDef *)self->usart;
    h.gState   = HAL_UART_STATE_READY;
    h.RxState  = HAL_UART_STATE_READY;
    return h;
}

void uart_send(UartPort *self, uint8_t byte)
{
    if (self->io) { self->io->write(byte); return; }
    UART_HandleTypeDef h = _make_huart(self);
    if (HAL_UART_Transmit(&h, &byte, 1, 10) != HAL_OK) {
        /* fallback: raw register */
        USART_TypeDef *u = (USART_TypeDef *)self->usart;
        while (!(u->SR & USART_SR_TXE)) {}
        u->DR = byte;
    }
}

void uart_send_str(UartPort *self, const char *str)
    { while (*str) uart_send(self, (uint8_t)*str++); }

void uart_send_buf(UartPort *self, const uint8_t *data, size_t len)
    { for (size_t i = 0; i < len; i++) uart_send(self, data[i]); }

uint8_t uart_recv(UartPort *self, uint8_t *out)
{
    if (self->io) {
        if (self->io->available()) { *out = self->io->read(); return 1; }
        return 0;
    }
    UART_HandleTypeDef h = _make_huart(self);
    if (HAL_UART_Receive(&h, out, 1, 0) == HAL_OK) return 1;
    /* fallback: raw register */
    USART_TypeDef *u = (USART_TypeDef *)self->usart;
    if (u->SR & USART_SR_RXNE) { *out = (uint8_t)u->DR; return 1; }
    return 0;
}

size_t uart_available(UartPort *self)
{
    if (self->io) return self->io->available();
    return (((USART_TypeDef *)self->usart)->SR & USART_SR_RXNE) ? 1 : 0;
}
