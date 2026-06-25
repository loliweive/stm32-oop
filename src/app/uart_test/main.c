/**
 * Minimal raw-UART + LED blink test — no HAL, no FreeRTOS
 * Just checks: clock, GPIO, UART transmit works
 */
#include "stm32f103xb.h"

static void dly(volatile uint32_t n) { while(n--) __asm__("nop"); }

int main(void)
{
    /* HSI 8MHz is default after reset — no clock config needed */

    /* Enable GPIOA, GPIOC, USART1 */
    RCC->APB2ENR |= (1<<2) | (1<<4) | (1<<14);

    /* PC13 = LED (push-pull 50MHz) */
    GPIOC->CRH = (GPIOC->CRH & ~(0xF<<20)) | (0x3<<20);

    /* PA9 = TX (AF push-pull 50MHz), PA10 = RX (input float) */
    GPIOA->CRH = (GPIOA->CRH & ~(0xF<<4)) | (0xB<<4);   /* PA9 */
    GPIOA->CRH = (GPIOA->CRH & ~(0xF<<8)) | (0x4<<8);   /* PA10 */

    /* USART1: 115200 @ 8MHz HSI
       BRR = PCLK/baud = 8000000/115200 = 69 = 0x45 */
    USART1->BRR = 69;  /* (4<<4)|5 */
    USART1->CR1 = (1<<13) | (1<<3) | (1<<2);  /* UE, TE, RE */

    /* LED: flash 3 quick to confirm boot */
    for(int i=0;i<3;i++){
        GPIOC->BRR=(1<<13); dly(100000);  /* ON */
        GPIOC->BSRR=(1<<13); dly(100000); /* OFF */
    }

    /* Send test message */
    const char *msg = "\r\n=== UART Raw Test ===\r\n";
    for(const char *p=msg;*p;p++){
        while(!(USART1->SR & (1<<7))){}  /* wait TXE */
        USART1->DR = *p;
    }

    int cnt = 0;
    while(1){
        /* LED toggle */
        GPIOC->BRR=(1<<13); dly(200000);  /* ON */
        GPIOC->BSRR=(1<<13); dly(200000); /* OFF */

        /* Send counter */
        char buf[32];
        int n = cnt++;
        char *p = buf + sizeof(buf) - 1;
        *p = '\0';
        do { *--p = '0' + (n%10); n/=10; } while(n);
        *--p = '[';  /* just use the chars directly */
        /* Simple send */
        for(const char *s=p;*s;s++){
            while(!(USART1->SR & (1<<7))){}
            USART1->DR = *s;
        }
        while(!(USART1->SR & (1<<7))){}
        USART1->DR = ']';
        while(!(USART1->SR & (1<<7))){}
        USART1->DR = '\r';
        while(!(USART1->SR & (1<<7))){}
        USART1->DR = '\n';

        dly(3000000);  /* ~1s delay between outputs */
    }
}
