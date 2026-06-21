#include "list.h"

void list_init(List *list)
{
    list->head.next = &list->head;
    list->head.prev = &list->head;
    list->count = 0;
}

bool list_empty(const List *list)
{
    return list->head.next == &list->head;
}

size_t list_count(const List *list)
{
    return list->count;
}

void list_insert_after(ListNode *pos, ListNode *node)
{
    node->next = pos->next;
    node->prev = pos;
    pos->next->prev = node;
    pos->next = node;
}

void list_insert_before(ListNode *pos, ListNode *node)
{
    list_insert_after(pos->prev, node);
}

void list_remove(List *list, ListNode *node)
{
    node->prev->next = node->next;
    node->next->prev = node->prev;
    node->next = node;
    node->prev = node;
    if (list->count > 0) {
        list->count--;
    }
}

void list_push_back(List *list, ListNode *node)
{
    list_insert_before(&list->head, node);
    list->count++;
}

void list_push_front(List *list, ListNode *node)
{
    list_insert_after(&list->head, node);
    list->count++;
}

ListNode *list_pop_front(List *list)
{
    if (list_empty(list)) {
        return NULL;
    }
    ListNode *node = list->head.next;
    list_remove(list, node);
    return node;
}

ListNode *list_pop_back(List *list)
{
    if (list_empty(list)) {
        return NULL;
    }
    ListNode *node = list->head.prev;
    list_remove(list, node);
    return node;
}

ListNode *list_first(const List *list)
{
    return list_empty(list) ? NULL : list->head.next;
}

ListNode *list_last(const List *list)
{
    return list_empty(list) ? NULL : list->head.prev;
}
