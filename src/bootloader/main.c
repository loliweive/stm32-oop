/**
 * @file    main.c
 * @brief   OTA Bootloader — 固件更新入口
 *
 * 启动流程:
 *   1. 检查 OTA Metadata 是否有已验证的固件 → 直接跳转
 *   2. 等待 UART 上是否有 OTA HELLO 帧 (3 秒超时)
 *      - 有 → 进入固件接收模式 (ota_client)
 *      - 无 → 检查是否有旧固件可跳转, 否则死循环等待 OTA
 *
 * 构建: cmake -B build/bootloader -DBUILD_MODE=bootloader
 * 烧录: openocd ... -c "program build/bootloader/stm32-oop.elf verify reset exit"
 *
 * Bootloader 占用 Flash 0x08000000 - 0x08002000 (8KB)
 * 应用固件从 0x08002000 开始
 */

#include "stm32f103xb.h"
#include "ota_config.h"
#include "ota_flash.h"
#include "ota_transport.h"
#include "ota_transport_uart.h"
#include "ota_client.h"
#include "gpio.h"
#include "rcc.h"

/* ── UART 传输上下文 (静态分配) ──────────────────────────────── */
static UartXportCtx uart_ctx;
static OtaTransport transport;
static OtaClient    ota;

/* ── LED 指示 ─────────────────────────────────────────────────── */
static GpioPin led;

static void led_init(void)
{
    rcc_enable_gpio('C');
    GpioPin_ctor(&led, GPIOC, GPIO_PIN_13);
    gpio_set_mode(&led, GPIO_MODE_OUT_PP);
}

static void led_on(void)  { gpio_set(&led, 0); } /* PC13: 低电平点亮 */
static void led_off(void) { gpio_set(&led, 1); }

/** 快速闪烁: 错误指示 */
static void led_error(void)
{
    for (int i = 0; i < 10; i++) {
        led_on();  for (volatile int d = 0; d < 200000; d++) {}
        led_off(); for (volatile int d = 0; d < 200000; d++) {}
    }
}

/* ── 固件跳转 ─────────────────────────────────────────────────── */
/**
 * @brief 跳转到应用固件
 *
 * 1. 设置向量表偏移为应用起始地址
 * 2. 从应用向量表读取初始 SP 和 PC
 * 3. 设置 MSP, 跳转到 Reset_Handler
 *
 * 此函数不应返回。
 */
static void jump_to_application(uint32_t app_addr) __attribute__((noreturn));

static void jump_to_application(uint32_t app_addr)
{
    uint32_t *vector_table = (uint32_t *)app_addr;

    /* 检查栈顶地址是否在 SRAM 范围内 (0x20000000 - 0x20005000 for 20KB) */
    uint32_t sp = vector_table[0];
    if (sp < 0x20000000 || sp > 0x20005000) {
        /* 无效的向量表 — 可能没有固件, 死循环等待 */
        while (1) { __asm__ volatile("nop"); }
    }

    uint32_t pc = vector_table[1];

    /* 关闭所有中断 */
    __disable_irq();

    /* 设置向量表偏移 */
    SCB_VTOR = app_addr;

    /* 设置主栈指针 */
    __asm__ volatile("msr msp, %0" : : "r"(sp));

    /* 跳转到应用 Reset_Handler */
    void (*app_reset)(void) = (void (*)(void))pc;
    app_reset();

    /* 不应到达 */
    while (1) {}
}

/* ── 进度回调 ─────────────────────────────────────────────────── */
static void on_ota_progress(uint32_t received, uint32_t total)
{
    /* 接收过程中快速闪烁 LED */
    static int toggle = 0;
    toggle = !toggle;
    if (toggle) led_on(); else led_off();
    (void)received; (void)total;
}

/* ── 主入口 ───────────────────────────────────────────────────── */
int main(void)
{
    /* 时钟: HSI 8MHz (bootloader 不需要高速) */
    rcc_set_sysclk(RCC_HSI, 0);

    /* LED 初始化 */
    led_init();
    led_on(); /* 进入 bootloader — 点亮 LED */

    /* ── 检查是否有已验证的固件 ───────────────────────────── */
    OtaMetadata meta;
    ota_flash_read(OTA_METADATA_START, (uint8_t *)&meta, sizeof(meta));

    if (meta.magic == OTA_MAGIC && meta.state == OTA_STATE_VERIFIED) {
        /* 标记为已启动 */
        meta.state = OTA_STATE_BOOTED;
        ota_flash_init();
        ota_flash_erase_page(OTA_METADATA_START);
        ota_flash_write(OTA_METADATA_START, (const uint8_t *)&meta, sizeof(meta));
        ota_flash_lock();

        led_off();
        jump_to_application(OTA_APP_START);
        /* jump_to_application 不应返回, 如果返回说明没有有效固件 */
    }

    /* ── 初始化 OTA 传输 ──────────────────────────────────── */
    ota_transport_uart_create(&transport, &uart_ctx);
    ota_xport_init(&transport);

    /* 初始化 OTA 客户端 */
    ota_client_init(&ota, &transport);
    ota.on_progress = on_ota_progress;

    /* ── 等待 OTA HELLO: 3 秒超时 ─────────────────────────── */
    /* 闪烁 LED 表示等待 */
    for (int i = 0; i < 30; i++) {
        uint8_t buf[OTA_FRAME_MAX_SIZE];
        size_t n = ota_xport_recv(&transport, buf, sizeof(buf), 100);

        if (n > 0) {
            uint8_t type, seq;
            const uint8_t *payload;
            size_t plen;

            if (ota_frame_decode(buf, n, &type, &seq, &payload, &plen)) {
                if (type == OTA_CMD_HELLO) {
                    /* 收到 HELLO → 进入完整的 OTA 接收流程 */
                    goto start_ota;
                }
            }
        }

        /* 交替 LED */
        if (i & 1) led_on(); else led_off();
    }

    /* ── 超时: 检查是否有旧固件可跳转 ───────────────────── */
    if (meta.magic == OTA_MAGIC && meta.state == OTA_STATE_BOOTED) {
        /* 之前已启动过的固件 — 直接跳转 */
        led_off();
        jump_to_application(OTA_APP_START);
    }

    /* 没有固件 — 等待 OTA (永不超时, 持续等待) */
    led_error();
    goto wait_forever;

start_ota:
    led_on();
    ota_client_start(&ota);

    /* ── OTA 接收循环 ────────────────────────────────────── */
    while (ota_client_poll(&ota)) {
        /* 持续驱动状态机 */
    }

    if (ota_client_get_state(&ota) == OTA_STATE_COMPLETE) {
        /* 完成 → 软复位, 让 bootloader 二次启动后检测并跳转 */
        ota_client_boot(&ota);
    } else {
        /* 出错 → 错误指示, 等待重试 */
        led_error();
        goto wait_forever;
    }

wait_forever:
    /* 持续等待 OTA HELLO */
    while (1) {
        uint8_t buf[OTA_FRAME_MAX_SIZE];
        size_t n = ota_xport_recv(&transport, buf, sizeof(buf), 1000);
        if (n > 0) {
            uint8_t type, seq;
            const uint8_t *payload;
            size_t plen;
            if (ota_frame_decode(buf, n, &type, &seq, &payload, &plen)
                && type == OTA_CMD_HELLO) {
                ota_client_start(&ota);
                while (ota_client_poll(&ota)) {}
                if (ota_client_get_state(&ota) == OTA_STATE_COMPLETE) {
                    ota_client_boot(&ota);
                }
            }
        }
    }

    return 0;
}
