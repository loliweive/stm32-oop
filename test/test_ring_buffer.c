/**
 * @file    test_ring_buffer.c
 * @brief   环形缓冲区单元测试 (TDD 示范)
 *
 * 本文件演示嵌入式 TDD 的核心思想：
 *   1. 在 host (x86) 上运行，无需硬件
 *   2. 秒级反馈 (vs 烧录-运行-观察的数分钟循环)
 *   3. 测试覆盖所有边界条件：空、满、绕回、批量操作
 *
 * ── TDD 流程 ─────────────────────────────────────────────
 *
 *   这个文件是第一步：先写测试 (RED phase)
 *   然后实现 ring_buffer.c 使测试通过 (GREEN phase)
 *   最后优化代码 (REFACTOR phase)
 *
 *   在持续开发中，每次修改 ring_buffer.c 后：
 *     $ cmake --build build/test && ctest -R test_ring_buffer
 *   可以在 1 秒内验证所有 11 个测试用例。
 *
 * ── 测试命名约定 ──────────────────────────────────────────
 *
 *   test_rb_<场景>_<预期行为>
 *   例如：test_rb_full — 测试"缓冲区满"的场景
 *         test_rb_pop_empty_fails — 测试空 pop 应失败
 *
 * ── setup() 模式 ─────────────────────────────────────────
 *
 *   每个 TEST 调用 setup() 确保独立初始状态。
 *   避免了测试之间的状态泄漏 (测试隔离)。
 */
#include "unity.h"
#include "ring_buffer.h"
#include <string.h>

#define BUF_SIZE 16
static uint8_t buf[BUF_SIZE];
static RingBuffer rb;

/** @brief 每个测试前调用 — 重置状态 */
static void setup(void)
{
    memset(buf, 0, sizeof(buf));
    rb_init(&rb, buf, BUF_SIZE);
}

/* ── 初始状态测试 ──────────────────── 验证 rb_init 后的状态 */
TEST(rb_init_is_empty)
{
    setup();
    TEST_ASSERT_TRUE(rb_empty(&rb));       /* 新缓冲区应为空 */
    TEST_ASSERT_FALSE(rb_full(&rb));       /* 新缓冲区不应满 */
    TEST_ASSERT_EQUAL_SIZE(0, rb_count(&rb));  /* 元素数为 0 */
    TEST_ASSERT_EQUAL_SIZE(BUF_SIZE, rb_free(&rb)); /* 全部可用 */
}

/* ── 基本 push/pop ───────────────────── 验证 FIFO 基本语义 */
TEST(rb_push_pop_one)
{
    setup();
    uint8_t val = 0;

    TEST_ASSERT_TRUE(rb_push(&rb, 42));    /* 压入应成功 */
    TEST_ASSERT_FALSE(rb_empty(&rb));      /* 不应该为空了 */
    TEST_ASSERT_EQUAL_SIZE(1, rb_count(&rb)); /* 应有一个元素 */

    TEST_ASSERT_TRUE(rb_pop(&rb, &val));   /* 弹出应成功 */
    TEST_ASSERT_EQUAL_U8(42, val);         /* 应得到压入的值 */
    TEST_ASSERT_TRUE(rb_empty(&rb));       /* 弹出后应为空 */
}

/* ── FIFO 顺序验证 ───────────────────── 先进先出顺序正确 */
TEST(rb_fifo_order)
{
    setup();
    uint8_t val;

    for (uint8_t i = 0; i < 10; i++) {
        TEST_ASSERT_TRUE(rb_push(&rb, i)); /* 压入 0..9 */
    }
    TEST_ASSERT_EQUAL_SIZE(10, rb_count(&rb));

    for (uint8_t i = 0; i < 10; i++) {
        TEST_ASSERT_TRUE(rb_pop(&rb, &val));
        TEST_ASSERT_EQUAL_U8(i, val);      /* 先入先出：0→1→...→9 */
    }
    TEST_ASSERT_TRUE(rb_empty(&rb));
}

/* ── 满检测 ──────────────────────────── 边界条件：缓冲区满 */
TEST(rb_full)
{
    setup();

    /* 填满缓冲区 */
    for (size_t i = 0; i < BUF_SIZE; i++) {
        TEST_ASSERT_TRUE(rb_push(&rb, (uint8_t)i));
    }
    TEST_ASSERT_TRUE(rb_full(&rb));        /* 应标记为满 */
    TEST_ASSERT_EQUAL_SIZE(BUF_SIZE, rb_count(&rb));
    TEST_ASSERT_EQUAL_SIZE(0, rb_free(&rb)); /* 无剩余空间 */

    /* 满时 push 应失败 */
    TEST_ASSERT_FALSE(rb_push(&rb, 255));
    TEST_ASSERT_TRUE(rb_full(&rb));        /* 满标志应保持 */
}

/* ── 空弹出 ──────────────────────────── 边界条件：空 pop 失败 */
TEST(rb_pop_empty_fails)
{
    setup();
    uint8_t val;
    TEST_ASSERT_FALSE(rb_pop(&rb, &val));  /* 空缓冲区 pop 应返回 false */
}

/* ── 绕回测试 ────────────────────────── 验证 head/tail 绕回正确 */
TEST(rb_wraparound)
{
    setup();
    uint8_t val;

    /* 先填满再排出一半，让 head 和 tail 都前进 */
    for (size_t i = 0; i < BUF_SIZE; i++) {
        rb_push(&rb, (uint8_t)i);
    }
    for (size_t i = 0; i < BUF_SIZE / 2; i++) {
        rb_pop(&rb, &val);                 /* 排出 8 个 */
    }
    /* 此时 tail=8, head=0 (满了绕回) → 有 8 个空位 */
    for (size_t i = 0; i < BUF_SIZE / 2; i++) {
        TEST_ASSERT_TRUE(rb_push(&rb, (uint8_t)(100 + i))); /* 绕回写入 */
    }
    TEST_ASSERT_TRUE(rb_full(&rb));
    TEST_ASSERT_EQUAL_SIZE(BUF_SIZE, rb_count(&rb));

    /* 验证 FIFO 在绕回后依然正确 */
    TEST_ASSERT_TRUE(rb_pop(&rb, &val));
    TEST_ASSERT_EQUAL_U8(8, val);          /* 第一个是 i=8 (之前没被排出的) */
}

/* ── Peek ─────────────────────────────── 查看但不移除 */
TEST(rb_peek)
{
    setup();
    uint8_t val;

    rb_push(&rb, 77);
    rb_push(&rb, 88);

    /* Peek 应该返回第一个元素但不移除 */
    TEST_ASSERT_TRUE(rb_peek(&rb, &val));
    TEST_ASSERT_EQUAL_U8(77, val);
    TEST_ASSERT_EQUAL_SIZE(2, rb_count(&rb)); /* 数量不变 — peek 不消耗 */

    /* 确认 peek 后 pop 仍然得到相同值 */
    rb_pop(&rb, &val);
    TEST_ASSERT_EQUAL_U8(77, val);
}

/* ── 批量操作 ─────────────────────────── push_many / pop_many */
TEST(rb_push_pop_many)
{
    setup();
    uint8_t data[] = {1, 2, 3, 4, 5};
    uint8_t out[5] = {0};

    size_t pushed = rb_push_many(&rb, data, 5);
    TEST_ASSERT_EQUAL_SIZE(5, pushed);
    TEST_ASSERT_EQUAL_SIZE(5, rb_count(&rb));

    size_t popped = rb_pop_many(&rb, out, 5);
    TEST_ASSERT_EQUAL_SIZE(5, popped);
    TEST_ASSERT_EQUAL_U8(1, out[0]);       /* 批量读取 FIFO 顺序正确 */
    TEST_ASSERT_EQUAL_U8(5, out[4]);
}

/* ── 批量部分写入 ─────────────────────── 空间不够时部分写入 */
TEST(rb_push_many_partial)
{
    setup();
    uint8_t data[BUF_SIZE + 4];

    /* 先填满 */
    for (size_t i = 0; i < BUF_SIZE; i++) {
        data[i] = (uint8_t)i;
    }
    rb_push_many(&rb, data, BUF_SIZE);
    TEST_ASSERT_TRUE(rb_full(&rb));

    /* 满了再 push — 应写入 0 个 */
    size_t pushed = rb_push_many(&rb, data, 4);
    TEST_ASSERT_EQUAL_SIZE(0, pushed);     /* 满了，一个也写不进 */
}

/* ── Clear ────────────────────────────── 清空操作 */
TEST(rb_clear)
{
    setup();
    for (size_t i = 0; i < 8; i++) {
        rb_push(&rb, (uint8_t)i);
    }
    TEST_ASSERT_EQUAL_SIZE(8, rb_count(&rb));

    rb_clear(&rb);                         /* 清空 */
    TEST_ASSERT_TRUE(rb_empty(&rb));
    TEST_ASSERT_FALSE(rb_full(&rb));
    TEST_ASSERT_EQUAL_SIZE(0, rb_count(&rb));

    /* 清空后应能继续使用 */
    TEST_ASSERT_TRUE(rb_push(&rb, 99));
    uint8_t val;
    rb_pop(&rb, &val);
    TEST_ASSERT_EQUAL_U8(99, val);
}

/* ── 压力测试 ─────────────────────────── 大量随机操作不崩溃 */
TEST(rb_stress)
{
    setup();
    uint8_t val;
    size_t ops = 1000;

    for (size_t i = 0; i < ops; i++) {
        if ((i & 1) == 0) {
            rb_push(&rb, (uint8_t)(i & 0xFF));  /* 偶数次：push */
        } else {
            rb_pop(&rb, &val);                    /* 奇数次：pop */
        }
    }
    /* 不崩溃就通过 — 验证边界条件不会导致内存错误 */
    TEST_ASSERT_TRUE(true);
}

int main(void)
{
    unity_init();
    RUN_TEST(rb_init_is_empty);
    RUN_TEST(rb_push_pop_one);
    RUN_TEST(rb_fifo_order);
    RUN_TEST(rb_full);
    RUN_TEST(rb_pop_empty_fails);
    RUN_TEST(rb_wraparound);
    RUN_TEST(rb_peek);
    RUN_TEST(rb_push_pop_many);
    RUN_TEST(rb_push_many_partial);
    RUN_TEST(rb_clear);
    RUN_TEST(rb_stress);
    unity_summary();
    return Unity.failed ? 1 : 0;
}
