/**
 * @file    ring_buffer.c
 * @brief   环形缓冲区实现
 *
 * 核心算法：
 *   push: buf[head] = data; head = (head + 1) % capacity
 *   pop:  data = buf[tail]; tail = (tail + 1) % capacity
 *
 * 满/空判断：
 *   空 → !full && head == tail     (tail 追上了 head，且未满)
 *   满 → full                       (head 绕了一圈追上了 tail)
 *
 * 为什么需要 full 标志？
 *   如果只用 head==tail 判断，无法区分"全空"和"全满"
 *   (两者都是 head==tail)。full 标志解决了这个歧义。
 *
 * 为什么不用 count？这样不更好吗？
 *   维护 count 需要在 push/pop 时同时修改 count 和指针，
 *   对 ISR 不够友好。head/tail/full 方式只需要各自操作
 *   对应的指针，天然无竞争。
 *
 * 容量设计建议：
 *   取 2 的幂 (8, 16, 32, 64, 128...) 可以利用位掩码
 *   替代取模运算 (head = (head+1) & (capacity-1))，
 *   省一个除法指令。但这里用 % 保留可读性，编译器
 *   在 capacity 为编译时常量时通常会优化。
 */

#include "ring_buffer.h"
#include <string.h>  /* memset */

/**
 * @brief 初始化环形缓冲区
 *
 * 将结构体字段归零，绑定用户提供的存储空间。
 * 调用者负责保证 buf 的生命周期长于 RingBuffer 的使用周期。
 *
 * 注意：buf 不会被清零，旧数据可能残留。
 * 数据读取始终由 head/tail 控制，残留数据不可访问。
 */
void rb_init(RingBuffer *rb, uint8_t *buf, size_t capacity)
{
    rb->buf      = buf;
    rb->capacity = capacity;
    rb->head     = 0;      /* 写指针归零 */
    rb->tail     = 0;      /* 读指针归零 */
    rb->full     = false;  /* 初始为空 */
}

/**
 * @brief 判断是否为空
 *
 * 条件：未满 且 读写指针重合
 * 只有 tail 追上了 head 且未标记为满才是空
 */
bool rb_empty(const RingBuffer *rb)
{
    return (!rb->full && rb->head == rb->tail);
}

bool rb_full(const RingBuffer *rb)
{
    return rb->full;
}

/**
 * @brief 当前元素计数
 *
 * 时间复杂度 O(1)，无循环。
 * 分三种情况：
 *   1. full → capacity
 *   2. head >= tail (未绕回) → head - tail
 *   3. head < tail (已绕回) → capacity - tail + head
 */
size_t rb_count(const RingBuffer *rb)
{
    if (rb->full) {
        return rb->capacity;                 /* 满了就是全部 */
    }
    if (rb->head >= rb->tail) {
        return rb->head - rb->tail;          /* 正常区间 */
    }
    return rb->capacity - rb->tail + rb->head; /* 绕回区间 */
}

size_t rb_free(const RingBuffer *rb)
{
    return rb->capacity - rb_count(rb);      /* 总容量 - 已用 */
}

/**
 * @brief 压入一个字节 (ISR 安全)
 *
 * 流程：
 *   1. 检查是否已满 → 满则返回 false
 *   2. 写入 buf[head]
 *   3. head 前进 (取模绕回)
 *   4. 如果 head 追上了 tail → 标记 full
 *
 * ISR 安全性：
 *   生产者只写 head，消费者只读 tail，不冲突。
 *   full 标志只有 push 时可能设 true，pop 时可能设 false，
 *   不存在同时写同一个变量的情况。
 */
bool rb_push(RingBuffer *rb, uint8_t byte)
{
    if (rb->full) {
        return false;  /* 满了，丢弃数据 */
    }
    rb->buf[rb->head] = byte;
    rb->head = (rb->head + 1) % rb->capacity;  /* 取模绕回 */
    if (rb->head == rb->tail) {
        rb->full = true;  /* 绕回追上 tail → 已满 */
    }
    return true;
}

/**
 * @brief 弹出一个字节 (主循环调用)
 *
 * 流程：
 *   1. 检查是否为空 → 空则返回 false
 *   2. 读取 buf[tail]
 *   3. tail 前进 (取模绕回)
 *   4. 清除 full 标志 (弹出后必然不满)
 */
bool rb_pop(RingBuffer *rb, uint8_t *out)
{
    if (rb_empty(rb)) {
        return false;  /* 空的，没数据 */
    }
    *out = rb->buf[rb->tail];
    rb->tail = (rb->tail + 1) % rb->capacity;  /* 取模绕回 */
    rb->full = false;  /* 弹出后一定不满了 */
    return true;
}

/**
 * @brief 查看队首元素但不移除
 *
 * 与 rb_pop 的区别：只读不推进 tail。
 * 多次 peek 返回相同值。常用于协议解析 (先看帧头再决定读多少)。
 */
bool rb_peek(const RingBuffer *rb, uint8_t *out)
{
    if (rb_empty(rb)) {
        return false;
    }
    *out = rb->buf[rb->tail];  /* 只读，不推进 tail */
    return true;
}

/**
 * @brief 批量压入
 *
 * 逐字节压入，空间不足时只压入能装下的部分。
 * 返回值小于 n 表示缓冲区满了。
 *
 * 优化空间：如果确定有足够空间，可以一次 memcpy 然后调整 head。
 * 但需要处理绕回情况 (分两段拷贝)，且批量 memcpy 在 ISR 中不如
 * 单字节推入安全。保留逐字节实现以确保 ISR 兼容。
 */
size_t rb_push_many(RingBuffer *rb, const uint8_t *data, size_t n)
{
    size_t pushed = 0;
    while (pushed < n && rb_push(rb, data[pushed])) {
        pushed++;
    }
    return pushed;
}

/**
 * @brief 批量弹出
 *
 * 逐字节弹出。返回值小于 n 表示缓冲区已空。
 */
size_t rb_pop_many(RingBuffer *rb, uint8_t *out, size_t n)
{
    size_t popped = 0;
    while (popped < n && rb_pop(rb, &out[popped])) {
        popped++;
    }
    return popped;
}

/**
 * @brief 清空缓冲区
 *
 * 快速清空：将头尾指针归零，清除满标志。
 * 不需要清理 buf 中的旧数据 (它们不会被访问到)。
 * 时间复杂度 O(1)。
 */
void rb_clear(RingBuffer *rb)
{
    rb->head = 0;
    rb->tail = 0;
    rb->full = false;
}
