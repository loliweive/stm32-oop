/**
 * @file    list.h
 * @brief   侵入式双向链表 / Intrusive Doubly-Linked List
 *
 * 这是 Linux 内核风格的侵入式链表。链表节点 (ListNode) 嵌入在
 * 用户结构体中，无需单独分配节点内存。
 *
 * ── 侵入式 vs 非侵入式 ───────────────────────────────────
 *
 *   非侵入式 (如 STL std::list):
 *     struct MyData { int x; };
 *     struct ListNode { ListNode *next, *prev; void *data; }; // data 是单独分配的
 *     → 每个插入需要 malloc 一个节点，data 指针多一次间接访问
 *
 *   侵入式 (kernel style):
 *     struct MyData { int x; ListNode node; };  // node 是数据的一部分
 *     → 零额外分配，node 就在数据结构体内部
 *     → 用 LIST_ENTRY 宏从 node 指针推导出 MyData 指针
 *
 * ── 内存布局示例 ─────────────────────────────────────────
 *
 *   List list;
 *   struct { int id; ListNode n; } a = {1}, b = {2}, c = {3};
 *   list_push_back(&list, &a.n); list_push_back(&list, &b.n);
 *
 *   list.head → a.n ⇄ b.n ⇄ c.n ⇄ list.head (环)
 *
 * ── LIST_ENTRY 宏原理 ───────────────────────────────────
 *
 *   已知：ListNode* ptr (指向嵌入在 MyData 中的 node)
 *   求：  MyData* (包含该 node 的 MyData 的地址)
 *
 *   解法：ptr - offsetof(MyData, node)
 *   即：从 node 的地址减去 node 在结构体中偏移量 = 结构体首地址
 *
 * ── 使用示例 ────────────────────────────────────────────
 *
 *   typedef struct { int id; ListNode node; } Task;
 *   List task_list;
 *   LIST_INIT(task_list);
 *
 *   Task t1 = {.id = 1};
 *   list_push_back(&task_list, &t1.node);
 *
 *   ListNode *n;
 *   LIST_FOREACH(n, &task_list) {
 *       Task *t = LIST_ENTRY(n, Task, node);
 *       printf("Task %d\n", t->id);
 *   }
 */

#ifndef LIST_H
#define LIST_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 链表节点 — 嵌入在用户结构体中 */
typedef struct ListNode {
    struct ListNode *next;  /**< 后继节点 */
    struct ListNode *prev;  /**< 前驱节点 */
} ListNode;

/** @brief 链表头 — 环形双向链表 */
typedef struct {
    ListNode head;   /**< 哨兵节点 (不存储数据) */
    size_t   count;  /**< 节点计数 (O(1) 查询) */
} List;

/** @brief 静态初始化器 — 声明时使用 */
#define LIST_INIT(list) \
    { .head = { .next = &(list).head, .prev = &(list).head }, .count = 0 }

void list_init(List *list);
bool list_empty(const List *list);
size_t list_count(const List *list);

void list_insert_after(ListNode *pos, ListNode *node);
void list_insert_before(ListNode *pos, ListNode *node);

/** @brief 从链表中移除节点并递减计数 */
void list_remove(List *list, ListNode *node);

void list_push_back(List *list, ListNode *node);
void list_push_front(List *list, ListNode *node);
ListNode *list_pop_front(List *list);
ListNode *list_pop_back(List *list);
ListNode *list_first(const List *list);
ListNode *list_last(const List *list);

/**
 * @brief 遍历链表
 * @param iter     ListNode* 迭代变量
 * @param list_ptr List* 链表指针
 *
 * 使用示例：
 *   ListNode *n;
 *   LIST_FOREACH(n, &my_list) {
 *       MyStruct *s = LIST_ENTRY(n, MyStruct, node);
 *   }
 */
#define LIST_FOREACH(iter, list_ptr) \
    for ((iter) = (list_ptr)->head.next; \
         (iter) != &((list_ptr)->head);  \
         (iter) = (iter)->next)

/**
 * @brief 从 ListNode 指针获取容器结构体指针
 * @param ptr    ListNode* 指针
 * @param type   容器结构体类型
 * @param member ListNode 在容器中的字段名
 * @return 容器结构体指针
 */
#define LIST_ENTRY(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))

#ifdef __cplusplus
}
#endif

#endif /* LIST_H */
