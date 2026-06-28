/**
 * FreeRTOS + raw UART — minimal test
 * Proves FreeRTOS + raw register UART works together
 */
#include <stdint.h>

#define PERIPH_BASE       0x40000000UL
#define APB2PERIPH_BASE   (PERIPH_BASE + 0x10000)
#define AHBPERIPH_BASE    (PERIPH_BASE + 0x20000)
#define RCC_BASE          (AHBPERIPH_BASE + 0x1000)
#define USART1_BASE       (APB2PERIPH_BASE + 0x3800)
#define GPIOA_BASE        (APB2PERIPH_BASE + 0x0800)
#define GPIOC_BASE        (APB2PERIPH_BASE + 0x1000)

#define RCC_APB2ENR       (*(volatile uint32_t*)(RCC_BASE + 0x18))
#define USART1_SR         (*(volatile uint32_t*)(USART1_BASE + 0x00))
#define USART1_DR         (*(volatile uint32_t*)(USART1_BASE + 0x04))
#define USART1_BRR        (*(volatile uint32_t*)(USART1_BASE + 0x08))
#define USART1_CR1        (*(volatile uint32_t*)(USART1_BASE + 0x0C))
#define GPIOA_CRH         (*(volatile uint32_t*)(GPIOA_BASE + 0x04))
#define GPIOC_CRH         (*(volatile uint32_t*)(GPIOC_BASE + 0x04))
#define GPIOC_BSRR        (*(volatile uint32_t*)(GPIOC_BASE + 0x10))

#include "FreeRTOS.h"
#include "task.h"

static void raw_putc(char c) { while(!(USART1_SR & (1<<7))){} USART1_DR=c; }
static void raw_puts(const char *s) { while(*s) raw_putc(*s++); }

static void task_serial(void *p) {
    (void)p;

    /* RCC: GPIOA + GPIOC + USART1 */
    RCC_APB2ENR |= (1<<2) | (1<<4) | (1<<14);

    /* PA9 TX, PA10 RX */
    GPIOA_CRH = (GPIOA_CRH & ~(0xFUL<<4))  | (0xBUL<<4);
    GPIOA_CRH = (GPIOA_CRH & ~(0xFUL<<8))  | (0x4UL<<8);

    /* USART1 115200@8MHz */
    USART1_BRR = (8000000UL + 115200/2) / 115200;
    USART1_CR1 = (1<<13) | (1<<3) | (1<<2);

    /* PC13 LED */
    GPIOC_CRH = (GPIOC_CRH & ~(0xFUL<<20)) | (0x3UL<<20);

    raw_puts("\r\n=== FreeRTOS + Raw UART OK ===\r\n> ");

    while(1) {
        if(USART1_SR & (1<<5)) {
            char c = (char)(USART1_DR & 0xFF);
            if(c == '\r') { raw_puts("\r\n> "); }
            else { raw_putc(c); }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

int main(void) {
    xTaskCreate(task_serial, "serial", 512, NULL, 1, NULL);
    vTaskStartScheduler();
    while(1) {}
}
