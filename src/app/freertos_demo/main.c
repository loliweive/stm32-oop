/**
 * @file    main.c
 * @brief   FreeRTOS 多任务实验 — LED 闪烁 + UART 回显
 *
 * 两个独立任务：
 *   Task LED:  以 500ms 周期闪烁 PC13 (板载 LED)
 *   Task UART: 回显 USART1 接收的每个字节
 *
 * 演示 FreeRTOS 的核心概念：
 *   1. 任务创建 (task_create)
 *   2. 优先级抢占 (LED 优先级 1, UART 优先级 2)
 *   3. 阻塞延时 (vTaskDelay — 不占用 CPU)
 *   4. 多任务协作 (两个任务独立运行, 互不干扰)
 *
 * ── 硬件连接 ─────────────────────────────────────────────
 *   LED:  PC13 (板载, 无需额外连接)
 *   UART: PA9=TX, PA10=RX (连接 USB 转串口模块)
 *         波特率 115200-8N1
 *
 * ── 构建与烧录 ───────────────────────────────────────────
 *   cmake -B build/freertos -DBUILD_MODE=freertos
 *   cmake --build build/freertos
 *   openocd -f interface/stlink.cfg -f target/stm32f1x.cfg \
 *     -c "program build/freertos/stm32-oop.elf verify reset exit"
 *
 * ── 预期行为 ─────────────────────────────────────────────
 *   1. 上电后 LED 开始以 1Hz 闪烁
 *   2. 通过串口发送任意字符, 会收到回显
 *   3. 两个任务同时运行, 互不阻塞
 *   4. 发送 '!' 字符会打印任务运行统计
 */

#include "stm32f103xb.h"
#include "FreeRTOS.h"
#include "task.h"
#include "freertos_scheduler.h"
#include "rcc.h"
#include "gpio.h"
#include "uart.h"

/* ── 外设 ──────────────────────────────────────────────────── */
static GpioPin led;
static UartPort uart;

/* ═══════════════════════════════════════════════════════════════
 *  Task LED — 周期闪烁板载 LED
 * ═══════════════════════════════════════════════════════════════ */
static void task_led(void *params)
{
    (void)params;

    /* 配置 LED 引脚: PC13, 推挽输出 */
    GpioPin_ctor(&led, GPIOC, GPIO_PIN_13);
    gpio_set_mode(&led, GPIO_MODE_OUT_PP);

    while (1) {
        gpio_toggle(&led);                /* 翻转 LED */
        vTaskDelay(pdMS_TO_TICKS(500));   /* 阻塞 500ms — 不占 CPU！ */
    }
}

/* ═══════════════════════════════════════════════════════════════
 *  Task UART — 串口回显
 * ═══════════════════════════════════════════════════════════════ */
static void task_uart(void *params)
{
    (void)params;

    /* 配置 USART1: PA9=TX, PA10=RX */
    GpioPin tx, rx;
    GpioPin_ctor(&tx, GPIOA, GPIO_PIN_9);
    gpio_set_mode(&tx, GPIO_CNF_ALT_PP | GPIO_MODE_OUT_50MHZ);

    GpioPin_ctor(&rx, GPIOA, GPIO_PIN_10);
    gpio_set_mode(&rx, GPIO_CNF_FLOAT | GPIO_MODE_IN);

    UartPort_ctor(&uart, USART1, 115200);
    uart_init(&uart);

    uart_send_str(&uart, "FreeRTOS UART Echo Ready\r\n");

    while (1) {
        uint8_t byte;
        if (uart_recv(&uart, &byte)) {
            /* 特殊命令: '!' 打印任务列表 */
            if (byte == '!') {
                uart_send_str(&uart, "\r\n--- Task List ---\r\n");
                /* vTaskList 需要足够大的缓冲区 */
                static char task_buf[256];
                vTaskList(task_buf);
                uart_send_str(&uart, task_buf);
                uart_send_str(&uart, "------------------\r\n");
            } else {
                uart_send(&uart, byte);   /* 回显 */
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));    /* 10ms 轮询间隔 */
    }
}

/* ═══════════════════════════════════════════════════════════════
 *  main — 初始化硬件, 创建任务, 启动调度器
 * ═══════════════════════════════════════════════════════════════ */
int main(void)
{
    /* 1. 时钟配置: HSE=8MHz → PLL×9 → 72MHz */
    rcc_set_sysclk(RCC_HSE, 9);

    /* 2. 使能外设时钟 */
    rcc_enable_gpio('A');   /* UART pins */
    rcc_enable_gpio('C');   /* LED pin */
    rcc_enable_usart(1);    /* USART1 */

    /* 3. 创建任务 — LED (优先级 1, 512 bytes 栈) */
    TaskParams led_task = {
        .func        = task_led,
        .name        = "LED",
        .stack_words = 128,
        .params      = NULL,
        .priority    = 1,
        .handle      = NULL,
    };
    task_create(&led_task);

    /* 4. 创建任务 — UART (优先级 2, 1KB 栈) */
    TaskParams uart_task = {
        .func        = task_uart,
        .name        = "UART",
        .stack_words = 256,
        .params      = NULL,
        .priority    = 2,
        .handle      = NULL,
    };
    task_create(&uart_task);

    /* 5. 启动调度器 — 永不返回 */
    freertos_start();

    /* 不会到这里 */
    return 0;
}
