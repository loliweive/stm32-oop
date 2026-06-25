/**
 * Minimal FreeRTOS + CLI test — HSI 8MHz, CLI only, no sensors/OLED/SPI
 */
#include "stm32f103xb.h"
#include "FreeRTOS.h"
#include "task.h"
#include "rcc.h"
#include "gpio.h"
#include "uart.h"
#include "cli.h"
#include <stdio.h>
#include <string.h>

/* ── Globals ───────────────────────────────────────────── */
static UartPort uart;
static GpioPin  led;
static CLI      cli;

static void uart_write(const char *s, size_t n) { uart_send_buf(&uart, (const uint8_t*)s, n); }

/* ── Commands ──────────────────────────────────────────── */
static void cmd_help(CLI *c, int a, char **v)   { (void)a;(void)v; cli_show_help(c); }
static void cmd_hello(CLI *c, int a, char **v)  { (void)a;(void)v; cli_printf(c, "Hello from FreeRTOS!\r\n"); }
static void cmd_uptime(CLI *c, int a, char **v) {
    (void)a;(void)v;
    uint32_t s = (uint32_t)(xTaskGetTickCount() / 1000);
    cli_printf(c, "Uptime: %lus\r\n", (unsigned long)s);
}
static void cmd_led(CLI *c, int a, char **v) {
    (void)a;(void)v;
    gpio_toggle(&led);
    cli_printf(c, "LED toggled\r\n");
}

static const CLICommand cmds[] = {
    {"help",   cmd_help,   "Show help"},
    {"hello",  cmd_hello,  "Say hello"},
    {"uptime", cmd_uptime, "Show uptime"},
    {"led",    cmd_led,    "Toggle LED"},
    {NULL,NULL,NULL}
};

/* ── CLI Task ──────────────────────────────────────────── */
static void dly(uint32_t n) { while(n--)__asm__("nop"); }

static void task_cli(void *p) {
    (void)p;

    /* LED flash: 3 quick = task running */
    for(int i=0;i<3;i++){gpio_set(&led,0);dly(500000);gpio_set(&led,1);dly(500000);}

    /* Init UART */
    { GpioPin t,r; GpioPin_ctor(&t,GPIOA,GPIO_PIN_9); gpio_set_mode(&t,GPIO_CNF_ALT_PP|GPIO_MODE_OUT_50MHZ);
      GpioPin_ctor(&r,GPIOA,GPIO_PIN_10); gpio_set_mode(&r,GPIO_CNF_FLOAT|GPIO_MODE_IN);
      UartPort_ctor(&uart,USART1,115200); uart_init(&uart); }

    cli_init(&cli, cmds, uart_write, NULL);
    cli_printf(&cli, "\r\n=== FreeRTOS CLI Minimal ===\r\n");
    cli_prompt(&cli);

    while(1) {
        uint8_t b;
        /* 排空 UART RX — 每字节到达需 ~87us@115200, 超时 1ms 无数据则退出 */
        for(int timeout=2000; timeout>0; timeout--) {
            if(uart_recv(&uart,&b)) {
                cli_feed(&cli,(char)b);
                timeout = 2000;  /* 收到数据, 重置超时 */
            }
            __asm__("nop");
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

/* ── Main ──────────────────────────────────────────────── */
int main(void)
{
    rcc_set_sysclk(RCC_HSI, 0);
    rcc_enable_gpio('A'); rcc_enable_gpio('C'); rcc_enable_usart(1);

    /* LED */
    GpioPin_ctor(&led, GPIOC, GPIO_PIN_13);
    gpio_set_mode(&led, GPIO_MODE_OUT_PP);

    /* LED: quick blink = init done, about to start scheduler */
    gpio_set(&led, 0); dly(2000000);  /* on ~0.8s @ 8MHz */
    gpio_set(&led, 1); dly(2000000);  /* off ~0.8s */

    /* Create CLI task */
    xTaskCreate(task_cli, "CLI", 512, NULL, 1, NULL);

    /* Start scheduler */
    vTaskStartScheduler();

    /* Should not reach — LED fast blink if it does */
    while(1){gpio_set(&led,0);dly(100000);gpio_set(&led,1);dly(100000);}
}
