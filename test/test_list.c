#include "unity.h"
#include "list.h"
#include <string.h>
#include <stdio.h>

/* Test struct that embeds a ListNode */
typedef struct {
    int id;
    ListNode node;
} TestItem;

/* ── Helpers ────────────────────────────────────────────────── */
static void setup_list(List *list) {
    list_init(list);
}

/* ── Init ───────────────────────────────────────────────────── */
TEST(list_init_empty)
{
    List list;
    setup_list(&list);
    TEST_ASSERT_TRUE(list_empty(&list));
    TEST_ASSERT_EQUAL_SIZE(0, list_count(&list));
    TEST_ASSERT_NULL(list_first(&list));
    TEST_ASSERT_NULL(list_last(&list));
}

/* ── Push / Pop ─────────────────────────────────────────────── */
TEST(list_push_back_pop_front)
{
    List list;
    setup_list(&list);
    TestItem items[3] = {{.id = 1}, {.id = 2}, {.id = 3}};

    for (int i = 0; i < 3; i++) {
        list_push_back(&list, &items[i].node);
    }
    TEST_ASSERT_EQUAL_SIZE(3, list_count(&list));
    TEST_ASSERT_FALSE(list_empty(&list));

    ListNode *n = list_pop_front(&list);
    TEST_ASSERT_NOT_NULL(n);
    TestItem *item = LIST_ENTRY(n, TestItem, node);
    TEST_ASSERT_EQUAL_U8(1, item->id);

    n = list_pop_front(&list);
    item = LIST_ENTRY(n, TestItem, node);
    TEST_ASSERT_EQUAL_U8(2, item->id);

    n = list_pop_front(&list);
    item = LIST_ENTRY(n, TestItem, node);
    TEST_ASSERT_EQUAL_U8(3, item->id);

    TEST_ASSERT_TRUE(list_empty(&list));
}

/* ── Push front / Pop back ──────────────────────────────────── */
TEST(list_push_front_pop_back)
{
    List list;
    setup_list(&list);
    TestItem items[2] = {{.id = 10}, {.id = 20}};

    list_push_front(&list, &items[0].node);
    list_push_front(&list, &items[1].node);  /* 20 before 10 */

    ListNode *n = list_pop_front(&list);
    TestItem *item = LIST_ENTRY(n, TestItem, node);
    TEST_ASSERT_EQUAL_U8(20, item->id);

    n = list_pop_front(&list);
    item = LIST_ENTRY(n, TestItem, node);
    TEST_ASSERT_EQUAL_U8(10, item->id);
}

/* ── Remove ─────────────────────────────────────────────────── */
TEST(list_remove_middle)
{
    List list;
    setup_list(&list);
    TestItem items[3] = {{.id = 1}, {.id = 2}, {.id = 3}};

    for (int i = 0; i < 3; i++) {
        list_push_back(&list, &items[i].node);
    }

    /* Remove middle */
    list_remove(&list, &items[1].node);
    TEST_ASSERT_EQUAL_SIZE(2, list_count(&list));

    ListNode *n = list_pop_front(&list);
    TestItem *item = LIST_ENTRY(n, TestItem, node);
    TEST_ASSERT_EQUAL_U8(1, item->id);

    n = list_pop_front(&list);
    item = LIST_ENTRY(n, TestItem, node);
    TEST_ASSERT_EQUAL_U8(3, item->id);
}

/* ── Foreach iteration ──────────────────────────────────────── */
TEST(list_foreach)
{
    List list;
    setup_list(&list);
    TestItem items[4] = {{.id = 0}, {.id = 1}, {.id = 2}, {.id = 3}};

    for (int i = 0; i < 4; i++) {
        list_push_back(&list, &items[i].node);
    }

    int count = 0;
    ListNode *n;
    LIST_FOREACH(n, &list) {
        TestItem *item = LIST_ENTRY(n, TestItem, node);
        TEST_ASSERT_EQUAL_U8(count, item->id);
        count++;
    }
    TEST_ASSERT_EQUAL_U8(4, count);
}

/* ── First / Last ───────────────────────────────────────────── */
TEST(list_first_last)
{
    List list;
    setup_list(&list);
    TestItem first = {.id = 100};
    TestItem last  = {.id = 99};

    list_push_back(&list, &first.node);
    list_push_back(&list, &last.node);

    ListNode *n = list_first(&list);
    TestItem *item = LIST_ENTRY(n, TestItem, node);
    TEST_ASSERT_EQUAL_U8(100, item->id);

    n = list_last(&list);
    item = LIST_ENTRY(n, TestItem, node);
    TEST_ASSERT_EQUAL_U8(99, item->id);
}

/* ── Empty pop ──────────────────────────────────────────────── */
TEST(list_pop_empty)
{
    List list;
    setup_list(&list);
    TEST_ASSERT_NULL(list_pop_front(&list));
    TEST_ASSERT_NULL(list_pop_back(&list));
}

/* ── Runner ─────────────────────────────────────────────────── */
int main(void)
{
    unity_init();
    RUN_TEST(list_init_empty);
    RUN_TEST(list_push_back_pop_front);
    RUN_TEST(list_push_front_pop_back);
    RUN_TEST(list_remove_middle);
    RUN_TEST(list_foreach);
    RUN_TEST(list_first_last);
    RUN_TEST(list_pop_empty);
    unity_summary();
    return Unity.failed ? 1 : 0;
}
