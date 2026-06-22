/**
 * @file    uart.c
 * @brief   UART 驱动实现 — 依赖注入 + 硬件操作双路径
 *
 * 核心设计：每个公开函数检查 self->io 是否非空。
 *   - io != NULL → 走注入接口 (测试/虚拟模式)
 *   - io == NULL → 走真实 USART 寄存器 (生产模式)
 *
 * 这个简单的 if 分支实现了依赖注入的核心思想：
 * 高层模块 (UartPort) 不直接依赖低层模块 (USART 寄存器)，
 * 而是依赖抽象接口 (SerialInterface)。
 *
 * ── USART 寄存器要点 (STM32F103) ─────────────────────────
 *
 *   SR:  状态寄存器
 *     TXE  (bit 7): 发送数据寄存器空 — 可以写下一个字节
 *     TC   (bit 6): 发送完成 — 最后一位已移出
 *     RXNE (bit 5): 接收数据寄存器非空 — 有数据可读
 *
 *   DR:  数据寄存器 (读写同一个地址，读=接收，写=发送)
 *   BRR: 波特率寄存器 = PCLK / (16 × baud)
 *         USART1 挂在 APB2 (72MHz)
 *         USART2/3 挂在 APB1 (36MHz)
 *   CR1: 控制寄存器 1 (UE=使能, TE=发送使能, RE=接收使能)
 *
 * ── 波特率计算 ───────────────────────────────────────────
 *   USARTDIV = PCLK / (16 × baud)
 *   例：72MHz / (16 × 115200) = 39.0625 → 39 (整数), 1 (小数)
 *   写入 BRR: (39 << 4) | 1 = 0x271
 *   这里使用简化整数除法 (四舍五入)。
 */

#include "uart.h"
#include "stm32f103xb.h"

/**
 * @brief 构造函数 — 初始化 UartPort
 *
 * 设置 USART 基址、波特率，初始化收发环形缓冲区。
 * 不操作硬件寄存器 (由 uart_init 完成)。
 *
 * 注意：rx_storage 和 tx_storage 的大小 (各 64 字节)
 * 是在结构体中静态分配的 — 零动态内存。
 */
void UartPort_ctor(UartPort *self, void *usart, uint32_t baud)
{
    self->usart    = usart;
    self->io       = NULL;      /* 默认使用真实硬件 */
    self->baudrate = baud;
    rb_init(&self->rx_buf, self->rx_storage, sizeof(self->rx_storage));
    rb_init(&self->tx_buf, self->tx_storage, sizeof(self->tx_storage));
}

/**
 * @brief 初始化 UART 硬件
 *
 * 配置波特率和使能收发。
 * 如果 io 已注入 (测试模式)，跳过硬件初始化。
 *
 * 注意：调用者需先配置 GPIO (PA9=TX, PA10=RX for USART1)
 * 和 RCC 时钟。本函数不负责 pin mux。
 */
void uart_init(UartPort *self)
{
    /* 依赖注入路径 — 无硬件初始化 */
    if (self->io) {
        return;
    }

    USART_Type *u = (USART_Type *)self->usart;

    /* 守卫: baudrate=0 会导致除零异常 */
    if (self->baudrate == 0) return;

    /* 计算 PCLK: USART1 在 APB2 (72MHz)，USART2/3 在 APB1 (36MHz) */
    uint32_t pclk = (u == USART1) ? 72000000 : 36000000;

    /* BRR = PCLK / baud (因为 BRR 编码的是 USARTDIV * 16 = PCLK / baud)
       参考手册公式: USARTDIV = PCLK / (16 × baud)
       但 BRR 寄存器存储 USARTDIV * 16 = PCLK / baud */
    u->BRR = (pclk + self->baudrate / 2) / self->baudrate;

    /* 使能 USART + 发送 + 接收 */
    u->CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;
}

/**
 * @brief 发送一个字节 (同步)
 *
 * 依赖注入分支：调用注入的 write() 函数。
 * 硬件分支：轮询 TXE 标志直到为空，然后写入 DR。
 *
 * 轮询方式在波特率 115200 时每个字节约 87µs。
 * 对于高速连续发送，应使用 DMA 或中断 (后续优化)。
 */
void uart_send(UartPort *self, uint8_t byte)
{
    if (self->io) {
        self->io->write(byte);   /* 注入路径 — mock/虚拟串口 */
        return;
    }

    /* 真实硬件路径 */
    USART_Type *u = (USART_Type *)self->usart;

    /* 等待发送数据寄存器为空 (TXE) */
    while (!(u->SR & USART_SR_TXE)) {
        /* 自旋等待 — 对于 115200 约 87µs/byte */
    }
    u->DR = byte;
}

/** @brief 发送字符串 — 逐字节委托给 uart_send */
void uart_send_str(UartPort *self, const char *str)
{
    while (*str) {
        uart_send(self, (uint8_t)*str++);
    }
}

/** @brief 发送字节数组 — 逐字节委托给 uart_send */
void uart_send_buf(UartPort *self, const uint8_t *data, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        uart_send(self, data[i]);
    }
}

/**
 * @brief 接收一个字节 (非阻塞)
 *
 * 依赖注入分支：检查 io->available()，读 io->read()。
 * 硬件分支：检查 RXNE 标志，读 DR。
 *
 * @return 1 获取到数据，0 无数据可读
 */
uint8_t uart_recv(UartPort *self, uint8_t *out)
{
    if (self->io) {
        if (self->io->available()) {
            *out = self->io->read();
            return 1;
        }
        return 0;  /* 无数据 */
    }

    /* 真实硬件路径 */
    USART_Type *u = (USART_Type *)self->usart;
    if (u->SR & USART_SR_RXNE) {
        *out = (uint8_t)(u->DR & 0xFF);
        return 1;
    }
    return 0;  /* 接收缓冲区为空 */
}

/**
 * @brief 查询可读字节数
 *
 * 注入路径：委托给 io->available()。
 * 硬件路径：简单检查 RXNE (返回 0 或 1，USART 硬件 FIFO 只有 1 字节)。
 *
 * 在中断驱动的实现中，这里可以返回环形缓冲区中的字节数。
 */
size_t uart_available(UartPort *self)
{
    if (self->io) {
        return self->io->available();
    }

    USART_Type *u = (USART_Type *)self->usart;
    return (u->SR & USART_SR_RXNE) ? 1 : 0;
}
