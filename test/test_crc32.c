/**
 * @file    test_crc32.c
 * @brief   CRC32 单元测试 — 已知向量验证 + 增量计算
 */
#include "unity.h"
#include "crc32.h"
#include <string.h>

/* ── 已知测试向量 ─────────────────────────────────────── */
TEST(crc32_empty)
{
    uint32_t crc = crc32_compute((const uint8_t *)"", 0);
    TEST_ASSERT_EQUAL_U32(0x00000000UL, crc);
}

TEST(crc32_known_vector_4bytes)
{
    uint8_t data[] = {0x00, 0x00, 0x00, 0x00};
    uint32_t crc = crc32_compute(data, sizeof(data));
    TEST_ASSERT_EQUAL_U32(0x2144DF1CUL, crc);
}

TEST(crc32_known_vector_123456789)
{
    /* RFC 1952 / Ethernet CRC32 standard test vector */
    uint32_t crc = crc32_compute((const uint8_t *)"123456789", 9);
    TEST_ASSERT_EQUAL_U32(0xCBF43926UL, crc);
}

TEST(crc32_ascii_hello)
{
    uint32_t crc = crc32_compute((const uint8_t *)"hello", 5);
    TEST_ASSERT_EQUAL_U32(0x3610A686UL, crc);
}

/* ── 增量计算 ────────────────────────────────────────── */
TEST(crc32_incremental_equals_single)
{
    const uint8_t *data = (const uint8_t *)"123456789";
    uint32_t single = crc32_compute(data, 9);

    uint32_t crc = 0xFFFFFFFFUL;
    crc = crc32_update(crc, data, 4);
    crc = crc32_update(crc, data + 4, 5);
    uint32_t incremental = crc ^ 0xFFFFFFFFUL;

    TEST_ASSERT_EQUAL_U32(single, incremental);
}

TEST(crc32_incremental_byte_by_byte)
{
    const uint8_t *data = (const uint8_t *)"hello";
    uint32_t single = crc32_compute(data, 5);

    uint32_t crc = 0xFFFFFFFFUL;
    for (size_t i = 0; i < 5; i++) {
        crc = crc32_update(crc, data + i, 1);
    }
    uint32_t incremental = crc ^ 0xFFFFFFFFUL;

    TEST_ASSERT_EQUAL_U32(single, incremental);
}

TEST(crc32_different_inputs_differ)
{
    uint32_t a = crc32_compute((const uint8_t *)"abc", 3);
    uint32_t b = crc32_compute((const uint8_t *)"abd", 3);
    TEST_ASSERT_NOT_EQUAL(a, b);
}

/* ── Runner ──────────────────────────────────────────── */
int main(void)
{
    unity_init();
    RUN_TEST(crc32_empty);
    RUN_TEST(crc32_known_vector_4bytes);
    RUN_TEST(crc32_known_vector_123456789);
    RUN_TEST(crc32_ascii_hello);
    RUN_TEST(crc32_incremental_equals_single);
    RUN_TEST(crc32_incremental_byte_by_byte);
    RUN_TEST(crc32_different_inputs_differ);
    return 0;
}
