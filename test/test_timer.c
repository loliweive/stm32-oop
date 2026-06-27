/** Timer HAL unit tests (host-side, using mock). */
#include "unity.h"
#include "mock_timer.h"
#include <string.h>

static Timer tim;
static MockTimerState mock;

static void setup(void)
{
    mock_timer_reset(&mock);
    memset(&tim, 0, sizeof(tim));
    Timer_ctor(&tim, NULL, 72000000);
    mock_timer_inject(&tim, &mock);
}

TEST(timer_init)
{
    setup();
    TEST_ASSERT_FALSE(mock.initialized);
    timer_init(&tim);
    TEST_ASSERT_TRUE(mock.initialized);
}

TEST(timer_start_stop)
{
    setup();
    TEST_ASSERT_FALSE(mock.running);
    timer_start(&tim);
    TEST_ASSERT_TRUE(mock.running);
    timer_stop(&tim);
    TEST_ASSERT_FALSE(mock.running);
}

TEST(timer_set_period)
{
    setup();
    timer_set_period_us(&tim, 1000);
    TEST_ASSERT_EQUAL_U32(1000, mock.period_us);
}

TEST(timer_set_pwm)
{
    setup();
    timer_set_pwm(&tim, 2, 5000); /* 50% on channel 2 */
    TEST_ASSERT_EQUAL_U16(5000, mock.pwm_duty[2]);
}

TEST(timer_ctor_sets_pclk)
{
    setup();
    TEST_ASSERT_EQUAL_U32(72000000, tim.pclk_hz);
}

int main(void)
{
    unity_init();
    RUN_TEST(timer_init);
    RUN_TEST(timer_start_stop);
    RUN_TEST(timer_set_period);
    RUN_TEST(timer_set_pwm);
    RUN_TEST(timer_ctor_sets_pclk);
    unity_summary();
    return Unity.failed ? 1 : 0;
}
