/**
 * @brief  外设测试: UART (PA9/PA10) — 回显
 * @usage  串口 115200-8N1, 敲任意键原样回显。Ctrl+C 无法退出用 screen 的 Ctrl+A K。
 *         正常=每个按键都被回显。
 */
#include "stm32f103xb.h"
#include "rcc.h"

int main(void) {
    /* 时钟 72MHz PLL */
    rcc_set_sysclk(RCC_PLL, 9);
    RCC->APB2ENR |= (1<<2) | (1<<4) | (1<<14);  /* GPIOA + GPIOC + USART1 */

    /* LED PC13 */
    GPIOC->CRH = (GPIOC->CRH & ~(0xF<<20)) | (0x3<<20);
    GPIOC->BSRR = (1<<13);  /* 灭 */

    /* PA9 TX (AF push-pull), PA10 RX (input pull-up) */
    GPIOA->CRH = (GPIOA->CRH & ~((0xF<<4)|(0xF<<8))) | (0xB<<4) | (0x8<<8);
    GPIOA->BSRR = (1<<10);  /* PA10 pull-up */

    /* USART1 115200 @ 72MHz APB2 */
    USART1->BRR = 625;  /* 72000000/115200 = 625 */
    USART1->CR1 = (1<<13) | (1<<3) | (1<<2) | (1<<5);  /* UE+TE+RE+RXNEIE */
    USART1->CR2 = 0;
    USART1->CR3 = 0;

    /* SysTick 1ms (72MHz → 72000 ticks/ms) */
    SysTick->LOAD = 72000 - 1;
    SysTick->VAL  = 0;
    SysTick->CTRL = 5;  /* HCLK/1, no interrupt */

    /* 闪灯 1 次: 就绪 (~500ms) */
    GPIOC->BSRR = (1<<29);  /* 亮 */
    for (volatile uint32_t i = 0; i < 36; i++)  /* 36 × 0xFFFFFF ≈ 500ms */
        while (!(SysTick->CTRL & (1<<16))) {}
    GPIOC->BSRR = (1<<13);  /* 灭 */

    while (1) {
        /* 收到字节 → 回显 */
        if (USART1->SR & (1<<5)) {  /* RXNE */
            uint8_t c = (uint8_t)USART1->DR;
            while (!(USART1->SR & (1<<7))) {}  /* TXE */
            USART1->DR = c;
            if (c == '\r') {  /* 回车补换行 */
                while (!(USART1->SR & (1<<7))) {}
                USART1->DR = '\n';
            }
        }
        /* LED 心跳: 1Hz 闪烁 (SysTick COUNTFLAG 轮询) */
        static uint32_t ms_tick = 0;
        if (SysTick->CTRL & (1<<16)) {  /* COUNTFLAG — 1ms 过去了 */
            if (++ms_tick >= 1000) {
                ms_tick = 0;
                GPIOC->ODR ^= (1<<13);  /* 翻转 LED */
            }
        }
    }
}
