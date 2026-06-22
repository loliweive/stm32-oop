/**
 * Minimal embedded Unity Test Framework.
 * Single-header version for stm32-oop.
 *
 * Unity is MIT-licensed by ThrowTheSwitch.org.
 * This is a lightweight port sufficient for our test suite.
 */

#ifndef UNITY_H
#define UNITY_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    size_t total;
    size_t passed;
    size_t failed;
    size_t ignored;
    const char *current_test;
} UnityState;

extern UnityState Unity;

#define UNITY_OUTPUT_CHAR(a) putchar(a)

void unity_init(void);
void unity_begin(const char *name);
void unity_end(void);
void unity_summary(void);

/* Assertions */
void _unity_assert_equal_u32(uint32_t expected, uint32_t actual,
    const char *file, int line, const char *msg);
void _unity_assert_equal_u8(uint8_t expected, uint8_t actual,
    const char *file, int line, const char *msg);
void _unity_assert_equal_ptr(const void *expected, const void *actual,
    const char *file, int line, const char *msg);
void _unity_assert_equal_size(size_t expected, size_t actual,
    const char *file, int line, const char *msg);
void _unity_assert_true(bool actual,
    const char *file, int line, const char *msg);
void _unity_assert_false(bool actual,
    const char *file, int line, const char *msg);
void _unity_assert_str_equal(const char *expected, const char *actual,
    const char *file, int line, const char *msg);
void _unity_assert_not_equal_u32(uint32_t expected, uint32_t actual,
    const char *file, int line, const char *msg);
void _unity_assert_null(const void *ptr,
    const char *file, int line, const char *msg);
void _unity_assert_not_null(const void *ptr,
    const char *file, int line, const char *msg);

#define TEST_ASSERT_EQUAL_U32(expected, actual) \
    _unity_assert_equal_u32((expected), (actual), __FILE__, __LINE__, NULL)
#define TEST_ASSERT_EQUAL_U16(expected, actual) \
    _unity_assert_equal_u32((expected), (actual), __FILE__, __LINE__, NULL)
#define TEST_ASSERT_EQUAL_U8(expected, actual) \
    _unity_assert_equal_u8((expected), (actual), __FILE__, __LINE__, NULL)
#define TEST_ASSERT_NOT_EQUAL(expected, actual) \
    _unity_assert_not_equal_u32((expected), (actual), __FILE__, __LINE__, NULL)
#define TEST_ASSERT_EQUAL_PTR(expected, actual) \
    _unity_assert_equal_ptr((expected), (actual), __FILE__, __LINE__, NULL)
#define TEST_ASSERT_EQUAL_SIZE(expected, actual) \
    _unity_assert_equal_size((expected), (actual), __FILE__, __LINE__, NULL)
#define TEST_ASSERT_TRUE(condition) \
    _unity_assert_true((condition), __FILE__, __LINE__, #condition)
#define TEST_ASSERT_FALSE(condition) \
    _unity_assert_false((condition), __FILE__, __LINE__, #condition)
#define TEST_ASSERT_NULL(ptr) \
    _unity_assert_null((ptr), __FILE__, __LINE__, NULL)
#define TEST_ASSERT_NOT_NULL(ptr) \
    _unity_assert_not_null((ptr), __FILE__, __LINE__, NULL)
#define TEST_ASSERT_EQUAL_STRING(expected, actual) \
    _unity_assert_str_equal((expected), (actual), __FILE__, __LINE__, NULL)

/* Test runner */
#define TEST(name)  static void test_##name(void)
#define RUN_TEST(name) do { \
    unity_begin(#name); test_##name(); unity_end(); \
} while(0)

#ifdef __cplusplus
}
#endif

#endif /* UNITY_H */
