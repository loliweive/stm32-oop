/**
 * UART Echo — echoes back any received byte via USART1 (PA9=TX, PA10=RX).
 *
 * Connect USB-to-serial adapter:
 *   PA9 (TX) → RX of adapter
 *   PA10 (RX) → TX of adapter
 *   GND → GND
 *
 * Baud rate: 115200-8N1.
 */
#include "stm32f103xb.h"
#include "rcc.h"
#include "gpio.h"
#include "uart.h"

int main(void)
{
    rcc_set_sysclk(RCC_HSE, 9);
    rcc_enable_gpio('A');
    rcc_enable_usart(1);

    /* PA9 = USART1 TX (AF push-pull) */
    GpioPin tx;
    GpioPin_ctor(&tx, GPIOA, GPIO_PIN_9);
    gpio_set_mode(&tx, GPIO_CNF_ALT_PP | GPIO_MODE_OUT_50MHZ);

    /* PA10 = USART1 RX (input float) */
    GpioPin rx;
    GpioPin_ctor(&rx, GPIOA, GPIO_PIN_10);
    gpio_set_mode(&rx, GPIO_CNF_FLOAT | GPIO_MODE_IN);

    UartPort uart;
    UartPort_ctor(&uart, USART1, 115200);
    uart_init(&uart);

    uart_send_str(&uart, "STM32 UART Echo Ready\r\n");

    while (1) {
        uint8_t byte;
        if (uart_recv(&uart, &byte)) {
            uart_send(&uart, byte); /* Echo back */
        }
    }
    return 0;
}
