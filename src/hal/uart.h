/**
 * @file    uart.h
 * @brief   OOP UART 串口驱动 — 依赖注入范例
 *
 * 这是依赖注入 (Dependency Injection) 模式在 C 中的示范实现。
 * 通过 SerialInterface 接口，UartPort 的实现与硬件解耦，
 * 测试时可以注入 mock IO 函数。
 *
 * ── 依赖注入 vs 传统方式 ──────────────────────────────────
 *
 *   传统 (紧耦合)：
 *     void uart_send(uint8_t byte) {
 *         while (!(USART1->SR & USART_SR_TXE));  // 直接操作硬件
 *         USART1->DR = byte;
 *     }
 *     问题：无法在 host 上测试，因为 USART1 是硬件寄存器。
 *
 *   依赖注入 (松耦合)：
 *     void uart_send(UartPort *self, uint8_t byte) {
 *         if (self->io) { self->io->write(byte); return; }  // 走接口
 *         while (!(u->SR & USART_SR_TXE));                  // 真实硬件
 *         u->DR = byte;
 *     }
 *     测试时注入 mock_io → host 可测试 ✅
 *     运行时 io=NULL → 走硬件 ✅
 *
 * ── SerialInterface 设计 ─────────────────────────────────
 *
 *   抽象了三个最基本的串口操作：
 *     write(byte) — 发送一个字节
 *     read()       — 接收一个字节
 *     available()  — 是否有数据可读
 *
 *   这是一个"角色接口" (Role Interface) —
 *   只包含 UartPort 真正需要的操作，不引入不必要的依赖。
 *
 * ── 环形缓冲区整合 ─────────────────────────────────────
 *
 *   每个 UartPort 内嵌两个 RingBuffer：
 *     rx_buf: ISR 写入，主循环读取
 *     tx_buf: 主循环写入，ISR 或后台发送 (DMA 时可以缓冲)
 */

#ifndef UART_HAL_H
#define UART_HAL_H

#include <stdint.h>
#include <stddef.h>
#include "ring_buffer.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 串行 IO 抽象接口 / Serial Interface Abstraction
 *
 * 定义最基础的串口操作。任何实现了这三个函数的结构体
 * 都可以作为 UartPort 的底层 IO。
 *
 * 实现者：
 *   - 真实硬件：USART 寄存器操作
 *   - Mock：    内存队列 (用于 host 测试)
 *   - 虚拟串口：USB CDC、文件等
 *
 * 三个函数均为同步操作 (阻塞或立即返回)。
 */
typedef struct {
    /** @brief 发送一个字节 (可阻塞等待发送完成) */
    void    (*write)(uint8_t data);

    /** @brief 接收一个字节 (立即返回，无数据时返回 0) */
    uint8_t (*read)(void);

    /** @brief 检查是否有数据可读 (返回可读字节数) */
    uint8_t (*available)(void);
} SerialInterface;

/**
 * @brief UART 端口实例 / UART Port Instance
 *
 * 包含硬件标识、注入的 IO 接口、收发缓冲区。
 *
 * 内存布局：
 *   - 结构体本身: ~48 bytes
 *   - rx_storage + tx_storage: 128 bytes (默认，可调整)
 *   - RingBuffer 开销: 各自 24 bytes
 *
 * @field usart    USART 外设地址 (USART1/USART2/USART3)
 * @field io      注入的 IO 接口 (NULL = 使用硬件)
 * @field rx_buf  接收环形缓冲区 (带存储)
 * @field tx_buf  发送环形缓冲区 (带存储)
 * @field baudrate 波特率 (如 115200)
 */
typedef struct {
    void             *usart;    /**< USART 寄存器基址 */
    SerialInterface  *io;       /**< 注入的 IO 接口 (NULL=硬件) */
    RingBuffer        rx_buf;   /**< 接收 fifo */
    uint8_t           rx_storage[64]; /**< 接收缓冲区存储 */
    RingBuffer        tx_buf;   /**< 发送 fifo */
    uint8_t           tx_storage[64]; /**< 发送缓冲区存储 */
    uint32_t          baudrate; /**< 波特率 */
} UartPort;

/**
 * @brief 构造函数
 * @param self  未初始化的 UartPort
 * @param usart USART 外设基地址 (USART1/USART2/USART3)
 * @param baud  波特率 (如 115200)
 */
void UartPort_ctor(UartPort *self, void *usart, uint32_t baud);

/** @brief 初始化 UART 硬件 (配置波特率、使能收发) */
void uart_init(UartPort *self);

/** @brief 发送一个字节 */
void uart_send(UartPort *self, uint8_t byte);

/** @brief 发送字符串 (以 \\0 结尾) */
void uart_send_str(UartPort *self, const char *str);

/** @brief 发送字节数组 */
void uart_send_buf(UartPort *self, const uint8_t *data, size_t len);

/**
 * @brief 接收一个字节 (非阻塞)
 * @param out 输出指针
 * @return 1 收到数据，0 无数据
 */
uint8_t uart_recv(UartPort *self, uint8_t *out);

/** @brief 查询可读字节数 */
size_t uart_available(UartPort *self);

#ifdef __cplusplus
}
#endif

#endif /* UART_HAL_H */
