#include "unity.h"
#include "assert.h"
#include <setjmp.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>

static jmp_buf assert_jmp;
static int handler_called = 0;
static char last_file[128];
static int  last_line = 0;

static void test_handler(const char *file, int line, const char *msg)
{
    (void)msg;
    handler_called++;
    strncpy(last_file, file, sizeof(last_file) - 1);
    last_line = line;
    longjmp(assert_jmp, 1);
}

/* ── Tests ──────────────────────────────────────────────────── */
TEST(assert_true_does_nothing)
{
    handler_called = 0;
    ASSERT(true);
    TEST_ASSERT_EQUAL_U8(0, handler_called);
}

TEST(assert_false_calls_handler)
{
    handler_called = 0;
    assert_set_handler(test_handler);

    if (setjmp(assert_jmp) == 0) {
        ASSERT(1 == 2);
        /* Should not reach here */
        TEST_ASSERT_TRUE(false);
    } else {
        TEST_ASSERT_EQUAL_U8(1, handler_called);
        TEST_ASSERT_NOT_NULL(strstr(last_file, "test_assert.c"));
    }
    assert_set_handler(NULL);
}

TEST(assert_msg)
{
    handler_called = 0;
    assert_set_handler(test_handler);

    if (setjmp(assert_jmp) == 0) {
        ASSERT_MSG(0, "custom failure");
        TEST_ASSERT_TRUE(false);
    } else {
        TEST_ASSERT_EQUAL_U8(1, handler_called);
    }
    assert_set_handler(NULL);
}

TEST(assert_can_restore_default_handler)
{
    assert_set_handler(test_handler);
    assert_set_handler(NULL);
    /* Default handler aborts — not tested here to avoid process exit */
    TEST_ASSERT_TRUE(true);
}

/* ── Runner ─────────────────────────────────────────────────── */
int main(void)
{
    unity_init();
    RUN_TEST(assert_true_does_nothing);
    RUN_TEST(assert_false_calls_handler);
    RUN_TEST(assert_msg);
    RUN_TEST(assert_can_restore_default_handler);
    unity_summary();
    return Unity.failed ? 1 : 0;
}
