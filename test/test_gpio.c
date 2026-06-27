/**
 * @file    test_gpio.c
 * @brief   GPIO HAL 单元测试 — 验证 vtable 派发和 mock 行为
 *
 * 这是 vtable 模式测试的示范：
 *   1. 构造 GpioPin (绑定真实 vtable)
 *   2. 注入 mock vtable (替换为记录函数)
 *   3. 调用 gpio_* 函数
 *   4. 验证 mock 记录了正确的调用和参数
 *
 * ── 为什么不直接验证寄存器值？ ──────────────────────────
 *
 *   在 host (x86) 上没有 STM32F103 的 GPIO 外设，
 *   不能通过读取真实寄存器来验证。所以采用行为验证：
 *   "gpio_set(&led, 1) 应该调用 vtable->set(&led, 1)"
 *
 * ── TDD 价值 ─────────────────────────────────────────────
 *
 *   这个测试可以在 1 秒内验证：
 *   - vtable 派发逻辑正确
 *   - 6 个 GPIO 操作 (init/set/get/toggle/set_mode) 都能正确委派
 *   - mock 状态跟踪正确
 *
 *   如果用硬件验证，同样的测试需要：
 *   - 编译 → 烧录 → 接逻辑分析仪/万用表 → 观察 → 重复
 *   - 每个周期 30-60 秒
 *
 *   30x 开发效率提升。
 */
#include "unity.h"
#include "stm32f103xb.h"
#include "mock_gpio.h"
#include <string.h>

/* 测试实例 */
static GpioPin pin;
static MockGpioState mock;

/** @brief 每个测试前重置 mock 并创建新的 GPIO 引脚 */
static void setup(void)
{
    mock_gpio_reset(&mock);
    memset(&pin, 0, sizeof(pin));
    /* 构造引脚：PC13 (Blue Pill 板载 LED) */
    GpioPin_ctor(&pin, NULL, GPIO_PIN_13);
    /* 注入 mock vtable — 此后所有操作走 mock，不碰硬件 */
    mock_gpio_inject(&pin, &mock);
}

/* ── 初始化 ───────────────────────────── 验证 gpio_init 调用了 vtable->init */
TEST(gpio_init_sets_flag)
{
    setup();
    TEST_ASSERT_FALSE(mock.initialized);  /* 初始为未初始化 */
    gpio_init(&pin);                      /* 调用初始化 */
    TEST_ASSERT_TRUE(mock.initialized);   /* mock 应记录初始化 */
}

/* ── 设置高电平 ───────────────────────── 验证 gpio_set(1) */
TEST(gpio_set_high)
{
    setup();
    gpio_set(&pin, 1);                    /* 设为高电平 */
    TEST_ASSERT_EQUAL_U8(1, mock.level);  /* mock 记录电平为 1 */
    TEST_ASSERT_EQUAL_U8(1, mock.set_count); /* 调用了一次 set */
}

/* ── 设置低电平 ───────────────────────── 验证 gpio_set(0) */
TEST(gpio_set_low)
{
    setup();
    gpio_set(&pin, 0);
    TEST_ASSERT_EQUAL_U8(0, mock.level);
}

/* ── 翻转 ─────────────────────────────── 验证 toggle 翻转电平 */
TEST(gpio_toggle_flips)
{
    setup();
    gpio_set(&pin, 0);
    gpio_toggle(&pin);                    /* 0 → 1 */
    TEST_ASSERT_EQUAL_U8(1, mock.level);
    TEST_ASSERT_EQUAL_U8(1, mock.toggle_count);

    gpio_toggle(&pin);                    /* 1 → 0 */
    TEST_ASSERT_EQUAL_U8(0, mock.level);
    TEST_ASSERT_EQUAL_U8(2, mock.toggle_count);
}

/* ── 读取 ─────────────────────────────── 验证 gpio_get 返回 mock 中的电平 */
TEST(gpio_get_reads_level)
{
    setup();
    mock.level = 1;                       /* 手动设置 mock 状态 */
    TEST_ASSERT_EQUAL_U8(1, gpio_get(&pin));

    mock.level = 0;
    TEST_ASSERT_EQUAL_U8(0, gpio_get(&pin));
}

/* ── 设置模式 ─────────────────────────── 验证 set_mode */
TEST(gpio_set_mode_records)
{
    setup();
    gpio_set_mode(&pin, GPIO_MODE_OUT_PP); /* 设为推挽输出 */
    TEST_ASSERT_EQUAL_U8(GPIO_MODE_OUT_PP, mock.mode);
}

int main(void)
{
    unity_init();
    RUN_TEST(gpio_init_sets_flag);
    RUN_TEST(gpio_set_high);
    RUN_TEST(gpio_set_low);
    RUN_TEST(gpio_toggle_flips);
    RUN_TEST(gpio_get_reads_level);
    RUN_TEST(gpio_set_mode_records);
    unity_summary();
    return Unity.failed ? 1 : 0;
}
