#include "unity.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

UnityState Unity = {0};

static void pass(void) {
    Unity.passed++;
    printf("  PASS\n");
}

static void fail(const char *file, int line, const char *msg) {
    Unity.failed++;
    printf("  FAIL at %s:%d", file, line);
    if (msg) printf(" — %s", msg);
    printf("\n");
}

void unity_init(void) {
    memset(&Unity, 0, sizeof(Unity));
}

void unity_begin(const char *name) {
    Unity.total++;
    Unity.current_test = name;
    printf("[TEST] %s\n", name);
}

void unity_end(void) {
    if (Unity.current_test) {
        /* Test passed if no explicit FAIL was called */
    }
    Unity.current_test = NULL;
}

void unity_summary(void) {
    printf("\n=========================\n");
    printf("Tests: %zu total\n", Unity.total);
    printf("  PASSED: %zu\n", Unity.passed);
    printf("  FAILED: %zu\n", Unity.failed);
    if (Unity.failed) {
        printf("*** SOME TESTS FAILED ***\n");
    } else {
        printf("ALL TESTS PASSED\n");
    }
    printf("=========================\n");
}

void _unity_assert_equal_u32(uint32_t expected, uint32_t actual,
    const char *file, int line, const char *msg)
{
    if (expected != actual) {
        printf("  Expected %u, got %u\n", expected, actual);
        fail(file, line, msg);
    } else {
        pass();
    }
}

void _unity_assert_equal_u8(uint8_t expected, uint8_t actual,
    const char *file, int line, const char *msg)
{
    if (expected != actual) {
        printf("  Expected %u, got %u\n", (unsigned)expected, (unsigned)actual);
        fail(file, line, msg);
    } else {
        pass();
    }
}

void _unity_assert_equal_ptr(const void *expected, const void *actual,
    const char *file, int line, const char *msg)
{
    if (expected != actual) {
        printf("  Expected %p, got %p\n", expected, (void *)actual);
        fail(file, line, msg);
    } else {
        pass();
    }
}

void _unity_assert_equal_size(size_t expected, size_t actual,
    const char *file, int line, const char *msg)
{
    if (expected != actual) {
        printf("  Expected %zu, got %zu\n", expected, actual);
        fail(file, line, msg);
    } else {
        pass();
    }
}

void _unity_assert_true(bool actual,
    const char *file, int line, const char *msg)
{
    if (!actual) {
        printf("  Expected TRUE\n");
        fail(file, line, msg);
    } else {
        pass();
    }
}

void _unity_assert_false(bool actual,
    const char *file, int line, const char *msg)
{
    if (actual) {
        printf("  Expected FALSE\n");
        fail(file, line, msg);
    } else {
        pass();
    }
}

void _unity_assert_str_equal(const char *expected, const char *actual,
    const char *file, int line, const char *msg)
{
    if (!expected && !actual) { pass(); return; }
    if (!expected || !actual || strcmp(expected, actual) != 0) {
        printf("  Expected \"%s\", got \"%s\"\n",
            expected ? expected : "(null)",
            actual ? actual : "(null)");
        fail(file, line, msg);
    } else {
        pass();
    }
}

void _unity_assert_not_equal_u32(uint32_t expected, uint32_t actual,
    const char *file, int line, const char *msg)
{
    if (expected == actual) {
        printf("  Expected NOT %u, got %u\n", expected, actual);
        fail(file, line, msg);
    } else {
        pass();
    }
}

void _unity_assert_null(const void *ptr,
    const char *file, int line, const char *msg)
{
    if (ptr != NULL) {
        printf("  Expected NULL, got %p\n", (void *)ptr);
        fail(file, line, msg);
    } else {
        pass();
    }
}

void _unity_assert_not_null(const void *ptr,
    const char *file, int line, const char *msg)
{
    if (ptr == NULL) {
        printf("  Expected non-NULL\n");
        fail(file, line, msg);
    } else {
        pass();
    }
}
