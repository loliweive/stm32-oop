/**
 * @file    test_iwdg.c
 * @brief   IWDG OOP vtable 单元测试 (9 用例, host 运行)
 */
#include "unity.h"
#include "mock_iwdg.h"
#include <string.h>

static Iwdg wdog;
static MockIwdgState mock;

static void setup(void)
{
    mock_iwdg_reset(&mock);
    memset(&wdog, 0, sizeof(wdog));
    Iwdg_ctor(&wdog, (void *)0x40003000);  /* IWDG_BASE */
    mock_iwdg_inject(&wdog, &mock);
}

/* 1. init 正确设置 prescaler 和 reload */
TEST(iwdg_init_sets_config)
{
    setup();
    iwdg_init(&wdog, 6, 0xFFF);
    TEST_ASSERT_TRUE(mock.initialized);
    TEST_ASSERT_EQUAL_U8(6, mock.prescaler);
    TEST_ASSERT_EQUAL_U16(0xFFF, mock.reload);
}

/* 2. init 幂等 — 第二次 init 被忽略 */
TEST(iwdg_init_is_idempotent)
{
    setup();
    iwdg_init(&wdog, 6, 0xFFF);
    iwdg_init(&wdog, 3, 100);  /* 尝试改用不同参数 */
    TEST_ASSERT_EQUAL_U8(6, mock.prescaler);    /* 仍是第一次的值 */
    TEST_ASSERT_EQUAL_U16(0xFFF, mock.reload);
}

/* 3. feed 递增 feed_count */
TEST(iwdg_feed_increments_count)
{
    setup();
    iwdg_init(&wdog, 6, 0xFFF);
    iwdg_feed(&wdog);
    iwdg_feed(&wdog);
    iwdg_feed(&wdog);
    TEST_ASSERT_TRUE(mock.feed_count == 3);
}

/* 4. 未 init 时 feed 无效 */
TEST(iwdg_feed_ignored_before_init)
{
    setup();
    iwdg_feed(&wdog);
    iwdg_feed(&wdog);
    TEST_ASSERT_TRUE(mock.feed_count == 0);
}

/* 5. init 后 is_enabled 返回 1 */
TEST(iwdg_is_enabled_after_init)
{
    setup();
    iwdg_init(&wdog, 6, 0xFFF);
    TEST_ASSERT_EQUAL_U8(1, iwdg_is_enabled(&wdog));
}

/* 6. 未 init 时 is_enabled 返回 0 */
TEST(iwdg_is_enabled_before_init)
{
    setup();
    TEST_ASSERT_EQUAL_U8(0, iwdg_is_enabled(&wdog));
}

/* 7. 超时计算: PR=6/256, RLR=156 → expected ms */
TEST(iwdg_get_timeout_4s)
{
    setup();
    iwdg_init(&wdog, 6, 156);
    uint32_t expected = ((uint32_t)(156 + 1) * (4 << 6)) / 40;
    TEST_ASSERT_EQUAL_U32(expected, iwdg_get_timeout_ms(&wdog));
}

/* 8. 最大超时: PR=7, RLR=0xFFF */
TEST(iwdg_get_timeout_max)
{
    setup();
    iwdg_init(&wdog, 7, 0xFFF);
    uint32_t expected = ((uint32_t)(0xFFF + 1) * (4 << 7)) / 40;
    TEST_ASSERT_EQUAL_U32(expected, iwdg_get_timeout_ms(&wdog));
}

/* 9. 构造函数初始化 vtable 非 NULL */
TEST(iwdg_ctor_sets_vtable)
{
    setup();
    TEST_ASSERT_NOT_NULL(wdog.vtable);
    TEST_ASSERT_EQUAL_U8(0, wdog._init);
    TEST_ASSERT_NOT_NULL(wdog.iwdg);
}

int main(void)
{
    unity_init();
    RUN_TEST(iwdg_init_sets_config);
    RUN_TEST(iwdg_init_is_idempotent);
    RUN_TEST(iwdg_feed_increments_count);
    RUN_TEST(iwdg_feed_ignored_before_init);
    RUN_TEST(iwdg_is_enabled_after_init);
    RUN_TEST(iwdg_is_enabled_before_init);
    RUN_TEST(iwdg_get_timeout_4s);
    RUN_TEST(iwdg_get_timeout_max);
    RUN_TEST(iwdg_ctor_sets_vtable);
    unity_summary();
    return Unity.failed ? 1 : 0;
}
