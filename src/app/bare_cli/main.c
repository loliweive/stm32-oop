/**
 * @file    main.c
 * @brief   裸机 CLI — UART 命令行 + DHT11 + OLED + SPI Flash
 *
 * 无 FreeRTOS, 纯轮询。CLI 引擎非阻塞模式。
 * 命令: help btn flash-id info led light oled runtime temp temp-stream
 */

#include "stm32f103xb.h"
#include "rcc.h"
#include "gpio.h"
#include "uart.h"
#include "i2c.h"
#include "cli.h"
#include "ssd1306.h"
#include "oled.h"
#include "spi_flash.h"
#include "dht11.h"
#include "light_sensor.h"
#include "iwdg.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static UartPort  uart;
static GpioPin   led, btn;
static SSD1306   oled;
static SpiFlash  spiflash;
static CLI       cli;
static DHT11     dht11;
static LightSensor light_obj;
static Sensor    *sensor, *light_sensor;
static bool      stream_mode = false;

static float    g_temp   = -273.0f;
static uint8_t  g_hum    = 255;
static bool     g_valid  = false;
static uint32_t g_ms     = 0;  /* uptime in ms (SysTick) */

static void delay_ms(uint32_t ms) {
    while (ms--) { for (volatile int i=0; i<7200; i++) __asm__("nop"); }
}

static void _uart_write(const char *s, size_t n) {
    uart_send_buf(&uart, (const uint8_t*)s, n);
}

/* ── CLI Commands ──────────────────────────────── */

static void cmd_help(CLI *c, int a, char **v)   { (void)a;(void)v; cli_show_help(c); }

static void cmd_btn(CLI *c, int a, char **v) {
    (void)a;(void)v;
    cli_printf(c, "PB14: %s\r\n", gpio_get(&btn)==0 ? "PRESSED" : "released");
}

static void cmd_led(CLI *c, int a, char **v) {
    if (a<2) { cli_printf(c, "Usage: led on|off|toggle\r\n"); return; }
    if (strcmp(v[1],"on")==0)      { gpio_set(&led,0); cli_printf(c,"LED ON\r\n"); }
    else if (strcmp(v[1],"off")==0){ gpio_set(&led,1); cli_printf(c,"LED OFF\r\n"); }
    else if (strcmp(v[1],"toggle")==0){gpio_toggle(&led); cli_printf(c,"LED toggled\r\n");}
}

static void cmd_runtime(CLI *c, int a, char **v) {
    (void)a;(void)v;
    uint32_t s = g_ms / 1000;
    cli_printf(c, "Runtime: %lus (%lu:%02lu:%02lu)\r\n",
        (unsigned long)s, (unsigned long)(s/3600),
        (unsigned long)((s%3600)/60), (unsigned long)(s%60));
}

static void cmd_temp(CLI *c, int a, char **v) {
    (void)a;(void)v;
    if (g_valid) {
        int ti=(int)g_temp, tf=(int)((g_temp-ti)*10);
        if(tf<0)tf=-tf;
        if(g_hum!=255)
            cli_printf(c, "DHT11: %d.%d C  %u %%RH\r\n", ti, tf, (unsigned)g_hum);
        else
            cli_printf(c, "DHT11: %d.%d C\r\n", ti, tf);
    } else cli_printf(c, "Sensor warming up...\r\n");
}

static void cmd_temp_stream(CLI *c, int a, char **v) {
    (void)a;(void)v;
    stream_mode = !stream_mode;
    cli_printf(c, stream_mode ? "Stream ON\r\n" : "Stream OFF\r\n");
}

static void cmd_info(CLI *c, int a, char **v) {
    (void)a;(void)v;
    cli_printf(c, "\r\n  MCU: STM32F103C8T6 @72MHz (bare-metal)\r\n");
    cli_printf(c, "  Uptime: %lus\r\n", (unsigned long)(g_ms/1000));
    const SpiFlashInfo *fi = spi_flash_get_info(&spiflash);
    cli_printf(c, "  Flash: %s %luKB\r\n", fi->name, (unsigned long)(fi->capacity/1024));
    cli_printf(c, "  Sensor: DHT11 + Light(M7)\r\n\r\n");
}

static void cmd_flash_id(CLI *c, int a, char **v) {
    (void)a;(void)v;
    const SpiFlashInfo *fi = spi_flash_get_info(&spiflash);
    cli_printf(c, "%s JEDEC: 0x%06lX %luKB\r\n",
        fi->name, (unsigned long)fi->jedec_id, (unsigned long)(fi->capacity/1024));
}

static void cmd_light(CLI *c, int a, char **v) {
    (void)a;(void)v;
    float lux; uint8_t dout;
    if (sensor_read(light_sensor, &lux, &dout)) {
        int pct=(int)lux, frac=(int)((lux-pct)*10);
        cli_printf(c, "Light(M7): %d.%d %%  DO=%u\r\n", pct, frac, (unsigned)dout);
    } else cli_printf(c, "Light read failed\r\n");
}

static void cmd_oled(CLI *c, int a, char **v) {
    (void)a;(void)v;
    OledDisplay *d = &oled.base;
    oled_clear(d);
    oled_show_string(d, 0, 0, "Bare-Metal CLI", OLED_FONT_8X16);
    oled_show_string(d, 0, 16, "DHT11+OLED", OLED_FONT_8X16);
    char buf[22]; snprintf(buf, sizeof(buf), "%lus", (unsigned long)(g_ms/1000));
    oled_show_string(d, 0, 32, buf, OLED_FONT_6X8);
    oled_flush(d);
    cli_printf(c, "OLED updated\r\n");
}

static const CLICommand cmds[] = {
    {"help",        cmd_help,        "Show help"},
    {"btn",         cmd_btn,         "Read PB14 button"},
    {"flash-id",    cmd_flash_id,    "SPI Flash JEDEC ID"},
    {"info",        cmd_info,        "System info"},
    {"led",         cmd_led,         "LED on|off|toggle"},
    {"light",       cmd_light,       "Read light sensor"},
    {"oled",        cmd_oled,        "Show info on OLED"},
    {"runtime",     cmd_runtime,     "Uptime"},
    {"temp",        cmd_temp,        "Read DHT11 once"},
    {"temp-stream", cmd_temp_stream, "Toggle DHT11 stream"},
    {NULL, NULL, NULL}
};

/* ── main ──────────────────────────────────────── */

int main(void) {
    rcc_set_sysclk(RCC_PLL, 9);
    SysTick->LOAD = 72000 - 1; SysTick->VAL = 0; SysTick->CTRL = 5;

    /* RAW UART test — after PLL, clock is 72MHz */
    RCC->APB2ENR |= (1<<2)|(1<<14);
    GPIOA->CRH = (GPIOA->CRH & ~(0xF<<4)) | (0xB<<4);
    USART1->BRR = 625; USART1->CR1 = (1<<13)|(1<<3);
    { char *m="\r\nBOOT\r\n"; while(*m){while(!(USART1->SR&(1<<7))){}USART1->DR=*m++;} }

    /* LED blink: prove we reach this point */
    rcc_enable_gpio('C'); GPIOC->CRH = (GPIOC->CRH & ~(0xF<<20)) | (0x3<<20);
    GPIOC->BSRR = (1<<29); for(volatile int i=0;i<500000;i++)__asm__("nop");
    GPIOC->BSRR = (1<<13);

    rcc_enable_gpio('A'); rcc_enable_gpio('B'); /* rcc_enable_gpio('C'); already done */
    rcc_enable_usart(1); rcc_enable_i2c(1); rcc_enable_spi(1); rcc_enable_adc(1);

    /* LED + Button */
    GpioPin_ctor(&led, GPIOC, GPIO_PIN_13); gpio_set_mode(&led, GPIO_MODE_OUT_PP); gpio_set(&led,1);
    GpioPin_ctor(&btn, GPIOB, GPIO_PIN_14); gpio_set_mode(&btn, 0x8|GPIO_MODE_IN); gpio_set(&btn,1);

    /* UART */
    { GpioPin t,r;
      GpioPin_ctor(&t,GPIOA,GPIO_PIN_9); gpio_set_mode(&t,GPIO_CNF_ALT_PP|GPIO_MODE_OUT_50MHZ);
      GpioPin_ctor(&r,GPIOA,GPIO_PIN_10); gpio_set_mode(&r,0x8|GPIO_MODE_IN); gpio_set(&r,1); }
    UartPort_ctor(&uart, USART1, 115200); uart_init(&uart);

    /* OLED */
    { GpioPin s,d;
      GpioPin_ctor(&s,GPIOB,GPIO_PIN_6); gpio_set_mode(&s,GPIO_CNF_ALT_OD|GPIO_MODE_OUT_50MHZ);
      GpioPin_ctor(&d,GPIOB,GPIO_PIN_7); gpio_set_mode(&d,GPIO_CNF_ALT_OD|GPIO_MODE_OUT_50MHZ); }
    { I2cPort ip; I2cPort_ctor(&ip, I2C1, 400000, 36000000); i2c_init(&ip); }
    ssd1306_ctor(&oled, I2C1, 0x3C); oled_init(&oled.base);

    /* SPI Flash */
    { GpioPin sck,miso,mosi,cs;
      GpioPin_ctor(&sck,GPIOA,GPIO_PIN_5); gpio_set_mode(&sck,GPIO_CNF_ALT_PP|GPIO_MODE_OUT_50MHZ);
      GpioPin_ctor(&miso,GPIOA,GPIO_PIN_6); gpio_set_mode(&miso,GPIO_CNF_FLOAT|GPIO_MODE_IN);
      GpioPin_ctor(&mosi,GPIOA,GPIO_PIN_7); gpio_set_mode(&mosi,GPIO_CNF_ALT_PP|GPIO_MODE_OUT_50MHZ);
      GpioPin_ctor(&cs,GPIOB,GPIO_PIN_9); gpio_set_mode(&cs,GPIO_MODE_OUT_PP); gpio_set(&cs,1); }
    spi_flash_init(&spiflash, SPI1, GPIOB, GPIO_PIN_9);

    /* Sensors */
    sensor = dht11_create(&dht11, GPIOA, GPIO_PIN_1);
    light_sensor = light_sensor_create(&light_obj, ADC1, 3, GPIOB, GPIO_PIN_11);
    delay_ms(1000);

    /* IWDG */
    iwdg_init(6, 0xFFF);

    /* CLI */
    cli_init(&cli, cmds, _uart_write, NULL);
    cli_printf(&cli, "\r\n=== Bare-Metal CLI ===\r\n");
    cli_printf(&cli, "Type 'help' for commands.\r\n");
    cli_prompt(&cli);

    uint32_t sensor_tick = 0, oled_tick = 0;

    while (1) {
        /* UART poll */
        uint8_t byte;
        while (uart_recv(&uart, &byte)) {
            if (stream_mode) { stream_mode=false; cli_printf(&cli,"\r\nStream stopped.\r\n"); cli_prompt(&cli); }
            else cli_feed(&cli, (char)byte);
        }

        /* SysTick timer */
        if (SysTick->CTRL & (1<<16)) g_ms++;

        /* Sensor (every 2s) */
        if (g_ms - sensor_tick >= 2000) {
            sensor_tick = g_ms;
            g_valid = sensor_read(sensor, &g_temp, &g_hum);
            if (g_valid && stream_mode) {
                int ti=(int)g_temp, tf=(int)((g_temp-ti)*10);
                if(tf<0)tf=-tf;
                cli_printf(&cli, "DHT: %d.%dC %u%%\r\n", ti, tf, (unsigned)g_hum);
            }
        }

        /* OLED (every 2s) */
        if (g_valid && g_ms - oled_tick >= 2000) {
            oled_tick = g_ms;
            OledDisplay *d = &oled.base;
            oled_clear(d);
            oled_show_string(d, 0, 0, "Bare-Metal", OLED_FONT_8X16);
            char buf[16]; int ti=(int)g_temp, tf=(int)((g_temp-ti)*10);
            if(tf<0)tf=-tf;
            snprintf(buf, sizeof(buf), "%d.%dC %u%%", ti, tf, (unsigned)g_hum);
            oled_show_string(d, 0, 16, buf, OLED_FONT_8X16);
            snprintf(buf, sizeof(buf), "%lus", (unsigned long)(g_ms/1000));
            oled_show_string(d, 0, 32, buf, OLED_FONT_6X8);
            oled_flush(d);
        }

        iwdg_feed();
    }
}
