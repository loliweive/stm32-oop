/**
 * Raw UART echo test — no HAL, no FreeRTOS, bare register writes
 * 用于验证 CH340 串口适配器和 PA9/PA10 接线是否正确
 *
 * 行为:
 *   1. LED (PC13) 快闪 3 次 = 启动成功
 *   2. 发送启动 banner
 *   3. 每 500ms 发一个 '.', 收到字符时回声
 *
 * HSI 8MHz, USART1 115200-8N1
 */
#include <stdint.h>

/* ── Peripheral base addresses ── */
#define PERIPH_BASE       0x40000000UL
#define APB2PERIPH_BASE   (PERIPH_BASE + 0x10000)
#define AHBPERIPH_BASE    (PERIPH_BASE + 0x20000)
#define RCC_BASE          (AHBPERIPH_BASE + 0x1000)
#define USART1_BASE       (APB2PERIPH_BASE + 0x3800)
#define GPIOA_BASE        (APB2PERIPH_BASE + 0x0800)
#define GPIOC_BASE        (APB2PERIPH_BASE + 0x1000)

#define RCC      ((volatile uint32_t*)RCC_BASE)
#define USART1_SR (*(volatile uint32_t*)(USART1_BASE + 0x00))
#define USART1_DR (*(volatile uint32_t*)(USART1_BASE + 0x04))
#define USART1_BRR (*(volatile uint32_t*)(USART1_BASE + 0x08))
#define USART1_CR1 (*(volatile uint32_t*)(USART1_BASE + 0x0C))
#define GPIOA_CRH (*(volatile uint32_t*)(GPIOA_BASE + 0x04))
#define GPIOC_CRH (*(volatile uint32_t*)(GPIOC_BASE + 0x04))
#define GPIOC_ODR (*(volatile uint32_t*)(GPIOC_BASE + 0x0C))
#define GPIOC_BSRR (*(volatile uint32_t*)(GPIOC_BASE + 0x10))

/* RCC register offsets */
#define RCC_APB2ENR (*(volatile uint32_t*)(RCC_BASE + 0x18))

void raw_putc(char c) {
    while(!(USART1_SR & (1<<7))) {}  /* wait TXE */
    USART1_DR = c;
}
void raw_puts(const char *s) { while(*s) raw_putc(*s++); }

void delay_ish(uint32_t n) { while(n--) __asm__("nop"); }

int main(void) {
    /* Enable GPIOA + GPIOC + USART1 clocks */
    RCC_APB2ENR |= (1<<2) | (1<<4) | (1<<14);

    /* ── LED (PC13) ── push-pull 50MHz */
    GPIOC_CRH = (GPIOC_CRH & ~(0xFUL << 20)) | (0x3UL << 20);

    /* ── UART pins (PA9=TX alt-pp, PA10=RX floating-input) ── */
    GPIOA_CRH = (GPIOA_CRH & ~(0xFUL << 4))  | (0xBUL << 4);   /* PA9 */
    GPIOA_CRH = (GPIOA_CRH & ~(0xFUL << 8))  | (0x4UL << 8);   /* PA10 */

    /* ── USART1: 115200 @ 8MHz → BRR ≈ 69 (USARTDIV * 16 rounded) ── */
    USART1_BRR = (8000000UL + 115200/2) / 115200;  /* ≈ 69 */
    USART1_CR1 = (1<<13) | (1<<3) | (1<<2);  /* UE + TE + RE */

    /* LED: 3 quick blinks = booted */
    for(int i=0; i<3; i++) {
        GPIOC_BSRR = (1<<13);  /* LED on (active low → set PC13=LOW) */
        delay_ish(200000);
        GPIOC_BSRR = (1<<(13+16));  /* LED off (reset PC13=HIGH) */
        delay_ish(200000);
    }

    /* Send banner */
    raw_puts("\r\n=== UART Raw Test ===\r\n");
    raw_puts("HSI 8MHz, USART1 115200-8N1\r\n");
    raw_puts("Send any char to echo:\r\n> ");

    int cnt = 0;
    while(1) {
        /* Echo RX */
        if(USART1_SR & (1<<5)) {  /* RXNE */
            char c = (char)(USART1_DR & 0xFF);
            if(c == '\r') { raw_puts("\r\n> "); }
            else { raw_putc(c); }
        }
        /* Heartbeat dot every ~500ms */
        cnt++;
        if(cnt > 360000) {  /* ~500ms @ 8MHz */
            cnt = 0;
            raw_putc('.');
            GPIOC_BSRR = (1<<(13+16)) | (1<<13);  /* toggle LED */
        }
    }
}
