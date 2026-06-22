/**
 * @file    main.c
 * @brief   DS18B20 温度传感器读取实验
 *
 * 每秒读取一次 DS18B20 温度, 通过 USART1 输出。
 *
 * 硬件连接:
 *   DS18B20 VDD  → 3.3V
 *   DS18B20 GND  → GND
 *   DS18B20 DQ   → PA1 (需要 4.7kΩ 上拉到 3.3V)
 *   USB-TTL RX   → PA9  (USART1 TX)
 *   USB-TTL TX   → PA10 (USART1 RX)
 *
 * 串口输出格式 (115200-8N1):
 *   Temp: +25.06 C  ROM: 28-xx-xx-xx-xx-xx-xx-xx
 *
 * 构建:
 *   cmake -B build/target -DBUILD_MODE=stm32f103
 *   cmake --build build/target
 */

#include "stm32f103xb.h"
#include "rcc.h"
#include "gpio.h"
#include "uart.h"
#include "onewire.h"
#include "ds18b20.h"
#include "sensor.h"
#include "delay.h"
#include <stdio.h>  /* snprintf */

/* ── 引脚配置 ────────────────────────────────────────────────────
 * 在这里修改你的实际接线:
 */
#define OW_PORT      GPIOA
#define OW_PIN       GPIO_PIN_1    /* PA1 = DS18B20 DQ */
#define UART_TX_PORT GPIOA
#define UART_TX_PIN  GPIO_PIN_9    /* PA9 = USART1 TX */
#define UART_RX_PORT GPIOA
#define UART_RX_PIN  GPIO_PIN_10   /* PA10 = USART1 RX */
/* ──────────────────────────────────────────────────────────────── */

static OneWireBus ow_bus;
static DS18B20    sensor;
static Sensor    *s;  /* 多态指针 */
static UartPort   uart;

int main(void)
{
    char buf[80];
    int len;

    /* 1. 时钟: 72MHz */
    rcc_set_sysclk(RCC_HSE, 9);
    rcc_enable_gpio('A');
    rcc_enable_usart(1);

    /* 2. 初始化 UART (调试输出) */
    {
        GpioPin tx, rx;
        GpioPin_ctor(&tx, UART_TX_PORT, UART_TX_PIN);
        gpio_set_mode(&tx, GPIO_CNF_ALT_PP | GPIO_MODE_OUT_50MHZ);
        GpioPin_ctor(&rx, UART_RX_PORT, UART_RX_PIN);
        gpio_set_mode(&rx, GPIO_CNF_FLOAT | GPIO_MODE_IN);

        UartPort_ctor(&uart, USART1, 115200);
        uart_init(&uart);
    }

    uart_send_str(&uart, "\r\n=== DS18B20 Temperature Reader ===\r\n");

    /* 3. 初始化 OneWire 总线 (PA1) */
    ow_init(&ow_bus, OW_PORT, OW_PIN);

    /* 4. 初始化 DS18B20 传感器 (OOP Sensor 接口) */
    Sensor *s = ds18b20_create(&sensor, &ow_bus);
    if (!s || !sensor_is_present(s)) {
        uart_send_str(&uart, "ERROR: DS18B20 not found!\r\n");
        uart_send_str(&uart, "Check: VDD=3.3V, DQ=PA1 with 4.7k pull-up\r\n");

        /* 错误: LED 快速闪烁 */
        rcc_enable_gpio('C');
        GpioPin led;
        GpioPin_ctor(&led, GPIOC, GPIO_PIN_13);
        gpio_set_mode(&led, GPIO_MODE_OUT_PP);

        while (1) {
            gpio_toggle(&led);
            delay_ms(200);  /* 快速闪烁指示错误 */
        }
    }

    /* 打印 ROM 码 */
    len = snprintf(buf, sizeof(buf),
                   "DS18B20 found. ROM: %02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X\r\n",
                   sensor.rom[0], sensor.rom[1], sensor.rom[2], sensor.rom[3],
                   sensor.rom[4], sensor.rom[5], sensor.rom[6], sensor.rom[7]);
    uart_send_buf(&uart, (uint8_t *)buf, len);

    /* 5. 主循环: 每秒读取温度 (多态!) */
    while (1) {
        float temp; uint8_t hum;
        if (sensor_read(s, &temp, &hum)) {
            len = snprintf(buf, sizeof(buf),
                           "Temp: %+7.2f C  | ROM: %02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X\r\n",
                           temp,
                           sensor.rom[0], sensor.rom[1], sensor.rom[2], sensor.rom[3],
                           sensor.rom[4], sensor.rom[5], sensor.rom[6], sensor.rom[7]);
            uart_send_buf(&uart, (uint8_t *)buf, len);
        } else {
            uart_send_str(&uart, "ERROR: CRC fail — read retry\r\n");
        }

        delay_ms(1000);  /* 每秒采样 */
    }

    return 0;
}
