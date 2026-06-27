/**
 * Minimal FreeRTOS + CLI test — HSI 8MHz, CLI + OLED
 */
#include "stm32f103xb.h"
#include "FreeRTOS.h"
#include "task.h"
#include "rcc.h"
#include "gpio.h"
#include "uart.h"
#include "i2c.h"
#include "ssd1306.h"
#include "oled.h"
#include "cli.h"
#include <stdio.h>
#include <string.h>

/* ── Globals ───────────────────────────────────────────── */
static UartPort uart;
static GpioPin  led;
static I2cPort  i2c;
static SSD1306  oled;
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

static void cmd_oled(CLI *c, int a, char **v) {
    (void)a;(void)v;
    OledDisplay *d = &oled.base;
    oled_clear(d);
    oled_show_string(d, 0,  0, "OLED Test OK!", OLED_FONT_8X16);
    oled_show_string(d, 0, 16, "STM32F103C8T6", OLED_FONT_8X16);
    oled_show_string(d, 0, 32, "FreeRTOS + CLI", OLED_FONT_8X16);
    oled_flush(d);
    cli_printf(c, "OLED updated\r\n");
}

static const CLICommand cmds[] = {
    {"help",   cmd_help,   "Show help"},
    {"hello",  cmd_hello,  "Say hello"},
    {"uptime", cmd_uptime, "Show uptime"},
    {"led",    cmd_led,    "Toggle LED"},
    {"oled",   cmd_oled,   "Test OLED display"},
    {NULL,NULL,NULL}
};

/* ── CLI Task ──────────────────────────────────────────── */
static void dly(uint32_t n) { while(n--)__asm__("nop"); }

/* Raw UART write — bypass HAL for debug */
#define RAW_SR (*(volatile uint32_t*)0x40013800UL)
#define RAW_DR (*(volatile uint32_t*)0x40013804UL)
static void raw_puts(const char *s) { while(*s) {while(!(RAW_SR & (1<<7))){} RAW_DR=*s++;} }

static void task_cli(void *p) {
    (void)p;

    /* ── RAW TEST: prove task is alive + TX works ── */
    /* Wait for LED init to finish, then send raw banner */

    /* LED flash: 3 quick = task running */
    for(int i=0;i<3;i++){gpio_set(&led,0);dly(50000);gpio_set(&led,1);dly(50000);}

    /* Init UART — raw register, same as working raw test */
    {
      /* PA9 TX, PA10 RX */
      GPIOA->CRH = (GPIOA->CRH & ~(0xFUL<<4))  | (0xBUL<<4);
      GPIOA->CRH = (GPIOA->CRH & ~(0xFUL<<8))  | (0x4UL<<8);
      /* USART1 115200@8MHz */
      USART1->BRR = (72000000UL + 115200/2) / 115200;
      USART1->CR1 = (1<<13) | (1<<3) | (1<<2);
    }

    /* ── RAW TEST: send raw banner before HAL ── */
    RAW_DR = '\r'; while(!(RAW_SR&(1<<7))){}
    RAW_DR = '\n'; while(!(RAW_SR&(1<<7))){}
    raw_puts("=== RAW: CLI task alive! ===");
    RAW_DR = '\r'; while(!(RAW_SR&(1<<7))){}
    RAW_DR = '\n'; while(!(RAW_SR&(1<<7))){}
    raw_puts("=== RAW: End ===\r\n");

    /* Also init HAL UART for CLI */
    UartPort_ctor(&uart,USART1,115200);

    /* ── Init I2C1 + OLED ── */
    rcc_enable_gpio('B');
    rcc_enable_i2c(1);
    { GpioPin scl,sda;
      GpioPin_ctor(&scl,GPIOB,GPIO_PIN_6); gpio_set_mode(&scl,GPIO_CNF_ALT_OD|GPIO_MODE_OUT_2MHZ);
      GpioPin_ctor(&sda,GPIOB,GPIO_PIN_7); gpio_set_mode(&sda,GPIO_CNF_ALT_OD|GPIO_MODE_OUT_2MHZ); }
    I2cPort_ctor(&i2c, I2C1, 100000, 36000000);  /* 100kHz, PCLK=8MHz */
    i2c_init(&i2c);
    ssd1306_ctor(&oled, I2C1, 0x3C);
    {
        OledDisplay *d = &oled.base;
        oled_init(d);
        oled_clear(d);
        oled_show_string(d, 0,  0, "OLED Ready", OLED_FONT_8X16);
        oled_show_string(d, 0, 24, "Type 'oled' to", OLED_FONT_6X8);
        oled_show_string(d, 0, 32, "test display", OLED_FONT_6X8);
        oled_flush(d);
    }

    cli_init(&cli, cmds, uart_write, NULL);
    cli_printf(&cli, "\r\n=== FreeRTOS CLI Minimal ===\r\n");
    cli_prompt(&cli);

    while(1) {
        uint8_t b;
        /* Busy-poll UART — 不 sleep，否则 1-byte FIFO 会丢字符 */
        if(uart_recv(&uart,&b)) {
            cli_feed(&cli,(char)b);
        }
    }
}

/* ── Main ──────────────────────────────────────────────── */
int main(void)
{
    /* ── ULTRA-EARLY raw UART — before any HAL ── */
    RCC->APB2ENR |= (1<<2) | (1<<4) | (1<<14);
    GPIOA->CRH = (GPIOA->CRH & ~(0xFUL<<4))  | (0xBUL<<4);
    GPIOA->CRH = (GPIOA->CRH & ~(0xFUL<<8))  | (0x4UL<<8);
    USART1->BRR = (72000000UL + 115200/2) / 115200;
    USART1->CR1 = (1<<13) | (1<<3) | (1<<2);
    raw_puts("\r\n=== MAIN START ===\r\n");

    rcc_set_sysclk(RCC_PLL, 9);
    rcc_enable_gpio('A'); rcc_enable_gpio('C'); rcc_enable_usart(1);

    /* LED */
    GpioPin_ctor(&led, GPIOC, GPIO_PIN_13);
    gpio_set_mode(&led, GPIO_MODE_OUT_PP);

    /* LED: quick blink = init done, about to start scheduler */
    gpio_set(&led, 0); dly(200000);  /* on ~80ms @ 8MHz */
    gpio_set(&led, 1); dly(200000);  /* off ~80ms */

    /* Create CLI task */
    xTaskCreate(task_cli, "CLI", 512, NULL, 1, NULL);  /* 512 words = 2KB, room for OLED */

    /* Start scheduler */
    vTaskStartScheduler();

    /* Should not reach — LED fast blink if it does */
    while(1){gpio_set(&led,0);dly(100000);gpio_set(&led,1);dly(100000);}
}
