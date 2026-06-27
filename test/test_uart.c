/**
 * @file    test_uart.c
 * @brief   UART HAL 单元测试 — 验证依赖注入模式
 *
 * 这是依赖注入测试的示范：
 *   1. 构造 UartPort
 *   2. 注入 mock SerialInterface (替换 USART 寄存器操作)
 *   3. 调用 uart_send/uart_recv/uart_available
 *   4. 验证 mock 的 tx_buf 收到了正确的数据
 *
 * ── 依赖注入的测试优势 ─────────────────────────────────
 *
 *   传统方式无法在 host 上测试 UART (需要真实串口硬件)
 *   通过 SerialInterface 抽象：
 *     - 测试时注入 mock → 操作内存队列，不需要硬件
 *     - 运行时 io=NULL → 自动走真实 USART 寄存器
 *   运行 switch 零开销 (一个 if 分支)
 */
#include "unity.h"
#include "mock_uart.h"
#include <string.h>

static UartPort uart;
static MockUartState mock;

static void setup(void)
{
    mock_uart_reset(&mock);
    memset(&uart, 0, sizeof(uart));
    UartPort_ctor(&uart, NULL, 115200);    /* 构造 UART (无真实硬件) */
    mock_uart_inject(&uart, &mock);         /* 注入 mock IO 接口 */
}

/* ── 发送单字节 ───────────────────────── 验证 uart_send 通过 mock 发送 */
TEST(uart_send_byte)
{
    setup();
    uart_send(&uart, 'A');                 /* 发送 'A' */
    TEST_ASSERT_EQUAL_U8(1, mock.tx_count);  /* 发送了一次 */
    TEST_ASSERT_EQUAL_U8('A', mock.tx_buf[0]); /* 内容是 'A' */
}

/* ── 发送字符串 ───────────────────────── 验证 uart_send_str */
TEST(uart_send_str)
{
    setup();
    uart_send_str(&uart, "Hello");         /* 发送 5 个字符 */
    TEST_ASSERT_EQUAL_U8(5, mock.tx_count);
    TEST_ASSERT_EQUAL_U8('H', mock.tx_buf[0]);
    TEST_ASSERT_EQUAL_U8('o', mock.tx_buf[4]);
}

/* ── 发送数组 ─────────────────────────── 验证 uart_send_buf */
TEST(uart_send_buf)
{
    setup();
    uint8_t data[] = {0xAA, 0xBB, 0xCC};
    uart_send_buf(&uart, data, 3);
    TEST_ASSERT_EQUAL_U8(3, mock.tx_count);
    TEST_ASSERT_EQUAL_U8(0xAA, mock.tx_buf[0]);
    TEST_ASSERT_EQUAL_U8(0xCC, mock.tx_buf[2]);
}

/* ── 接收数据 ─────────────────────────── 验证 uart_recv 读取 mock 中的字节 */
TEST(uart_recv_available)
{
    setup();
    mock_uart_rx_push(&mock, 42);          /* 向 mock rx 队列推入字节 */
    uint8_t val = 0;
    uint8_t ok = uart_recv(&uart, &val);
    TEST_ASSERT_EQUAL_U8(1, ok);           /* 应该收到 */
    TEST_ASSERT_EQUAL_U8(42, val);         /* 值正确 */
}

/* ── 空接收 ───────────────────────────── 无数据时 recv 返回 0 */
TEST(uart_recv_empty)
{
    setup();
    uint8_t val = 0xFF;
    uint8_t ok = uart_recv(&uart, &val);  /* 没有推入数据 */
    TEST_ASSERT_EQUAL_U8(0, ok);          /* 应返回 0 */
    TEST_ASSERT_EQUAL_U8(0xFF, val);      /* val 不应被修改 */
}

/* ── 无数据 available ──────────────────── 空队列时 available=0 */
TEST(uart_available_zero)
{
    setup();
    TEST_ASSERT_EQUAL_U8(0, uart_available(&uart));
}

/* ── 构造函数 ─────────────────────────── 验证波特率设置 */
TEST(uart_ctor_sets_baudrate)
{
    setup();
    TEST_ASSERT_EQUAL_U32(115200, uart.baudrate);
}

int main(void)
{
    unity_init();
    RUN_TEST(uart_send_byte);
    RUN_TEST(uart_send_str);
    RUN_TEST(uart_send_buf);
    RUN_TEST(uart_recv_available);
    RUN_TEST(uart_recv_empty);
    RUN_TEST(uart_available_zero);
    RUN_TEST(uart_ctor_sets_baudrate);
    unity_summary();
    return Unity.failed ? 1 : 0;
}
