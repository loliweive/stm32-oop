/**
 * @file    ring_buffer.h
 * @brief   环形缓冲区 / Circular FIFO Buffer
 *
 * 这是一个线程安全的环形缓冲区实现，专为嵌入式系统设计。
 * 单生产者 / 单消费者场景下无需加锁，ISR 中安全使用。
 * 所有操作均为 O(1) 时间复杂度，零动态内存分配。
 *
 * This is a thread-safe ring buffer designed for embedded systems.
 * Lock-free for single-producer / single-consumer scenarios.
 * All operations are O(1). No dynamic allocation.
 *
 * ── 使用场景 / Use Cases ──────────────────────────────────
 *  · UART 收发缓冲 (ISR → 主循环)
 *  · 传感器数据暂存
 *  · 命令队列
 *  · 任何 FIFO 场景
 *
 * ── 内存布局 / Memory Layout ─────────────────────────────
 *
 *    tail                 head
 *     ↓                    ↓
 *   ┌───┬───┬───┬───┬───┬───┐
 *   │ D │ D │ D │   │   │   │  D = 有效数据
 *   └───┴───┴───┴───┴───┴───┘
 *
 *   head == tail  → 空 (empty)
 *   head == tail && full → 满 (full)
 *   使用 full 标志区分"空"和"满"两种 head==tail 状态
 *
 * ── ISR 安全性 / ISR Safety ──────────────────────────────
 *   · rb_push(): ISR 安全 (单生产者)
 *   · rb_pop():  主循环调用 (单消费者)
 *   · 多生产者需外部同步
 */

#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 环形缓冲区结构体
 *
 * 用户提供存储空间 (buf)，结构体本身不持有内存。
 * 典型用法：声明一个 uint8_t 数组，传入 rb_init()。
 *
 * @field buf      用户提供的存储空间指针
 * @field capacity 总容量 (slots)
 * @field head     写指针 (生产者)
 * @field tail     读指针 (消费者)
 * @field full     满标志，用于区分 empty vs full
 */
typedef struct {
    uint8_t *buf;        /**< 调用者分配的存储空间 */
    size_t   capacity;   /**< 总容量 */
    size_t   head;       /**< 写索引 (生产者推进) */
    size_t   tail;       /**< 读索引 (消费者推进) */
    bool     full;       /**< 满标志：区分 empty 和 full */
} RingBuffer;

/**
 * @brief 初始化环形缓冲区
 * @param rb       未初始化的 RingBuffer 指针
 * @param buf      调用者提供的存储数组
 * @param capacity 数组大小 (最大存储元素数)
 */
void rb_init(RingBuffer *rb, uint8_t *buf, size_t capacity);

/** @brief 缓冲区是否为空 (无可读数据) */
bool rb_empty(const RingBuffer *rb);

/** @brief 缓冲区是否已满 (写入将被丢弃) */
bool rb_full(const RingBuffer *rb);

/** @brief 当前存储的元素数量 */
size_t rb_count(const RingBuffer *rb);

/** @brief 剩余可用空间 */
size_t rb_free(const RingBuffer *rb);

/**
 * @brief 压入一个字节
 * @param rb   缓冲区指针
 * @param byte 要写入的字节
 * @return true 成功，false 缓冲区已满
 * @note ISR 安全 (单生产者场景)
 */
bool rb_push(RingBuffer *rb, uint8_t byte);

/**
 * @brief 弹出一个字节
 * @param rb  缓冲区指针
 * @param out 读取数据输出
 * @return true 成功，false 缓冲区为空
 * @note 主循环调用，非 ISR
 */
bool rb_pop(RingBuffer *rb, uint8_t *out);

/**
 * @brief 查看下一个字节但不移除
 * @param rb  缓冲区指针
 * @param out 数据输出
 * @return true 有数据，false 为空
 */
bool rb_peek(const RingBuffer *rb, uint8_t *out);

/**
 * @brief 批量压入数据
 * @param rb   缓冲区指针
 * @param data 数据源
 * @param n    要写入的字节数
 * @return 实际写入的字节数 (如果空间不足，写入能装下的部分)
 */
size_t rb_push_many(RingBuffer *rb, const uint8_t *data, size_t n);

/**
 * @brief 批量弹出数据
 * @param rb  缓冲区指针
 * @param out 输出缓冲区
 * @param n   要读取的字节数
 * @return 实际读取的字节数 (如果数据不够，只读取存在的部分)
 */
size_t rb_pop_many(RingBuffer *rb, uint8_t *out, size_t n);

/** @brief 清空缓冲区 (重置 head/tail/full) */
void rb_clear(RingBuffer *rb);

#ifdef __cplusplus
}
#endif

#endif /* RING_BUFFER_H */
