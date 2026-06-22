/**
 * @file    dht11.c
 * @brief   DHT11 驱动实现 — GPIO 位操作 (关中断保证时序)
 */
#include "dht11.h"
#include "stm32f103xb.h"

/* ── 延时 (72MHz 校准) ────────────────────────────────────────── */
#define DELAY_US(us) do { \
    volatile uint32_t _c = (uint32_t)(us) * 6; \
    while (_c--) { __asm__ volatile("nop"); } \
} while(0)

/* ── GPIO 位操作 ──────────────────────────────────────────────── */
static inline void _pin_output(DHT11 *d) {
    GPIO_Type *g = (GPIO_Type *)d->port; uint16_t m = d->pin;
    uint32_t p=0; while (!(m&(1<<p))) p++;
    if (p<8) g->CRL=(g->CRL&~(0xFUL<<(p*4)))|(0x3UL<<(p*4));  /* PP 50MHz */
    else     g->CRH=(g->CRH&~(0xFUL<<((p-8)*4)))|(0x3UL<<((p-8)*4));
}
static inline void _pin_input(DHT11 *d) {
    GPIO_Type *g = (GPIO_Type *)d->port; uint16_t m = d->pin;
    uint32_t p=0; while (!(m&(1<<p))) p++;
    if (p<8) g->CRL=(g->CRL&~(0xFUL<<(p*4)))|(0x8UL<<(p*4));  /* input PU/PD */
    else     g->CRH=(g->CRH&~(0xFUL<<((p-8)*4)))|(0x8UL<<((p-8)*4));
    g->BSRR = m;  /* 启用上拉 */
}
static inline void _pin_low(DHT11 *d)   { ((GPIO_Type *)d->port)->BRR = d->pin; }
static inline void _pin_high(DHT11 *d)  { ((GPIO_Type *)d->port)->BSRR = d->pin; }
static inline uint8_t _pin_read(DHT11 *d) { return ((GPIO_Type *)d->port)->IDR & d->pin ? 1 : 0; }

/* ── 公开 API ──────────────────────────────────────────────────── */

void dht11_init(DHT11 *dht, void *port, uint16_t pin)
{
    dht->port    = port;
    dht->pin     = pin;
    dht->humidity = 0;
    dht->temp_c  = 0;
    dht->present = false;

    /* 总线初始状态: 上拉高 */
    _pin_output(dht);
    _pin_high(dht);
}

bool dht11_read(DHT11 *dht)
{
    uint8_t data[5] = {0};

    /* ── 1. 起始信号: 拉低 18ms ─────────────────────────── */
    __disable_irq();
    _pin_output(dht);
    _pin_low(dht);
    DELAY_US(18000);   /* 18ms 低电平 */
    _pin_high(dht);
    DELAY_US(30);       /* 30µs 高电平 */
    _pin_input(dht);   /* 释放总线, 等待 DHT11 应答 */

    /* ── 2. 等待 DHT11 应答 ──────────────────────────────── */
    /* 应答低电平: 80µs */
    uint32_t timeout = 10000;
    while (_pin_read(dht) == 1 && --timeout) {}
    if (timeout == 0) { __enable_irq(); dht->present = false; return false; }
    DELAY_US(80);

    /* 应答高电平: 80µs */
    timeout = 10000;
    while (_pin_read(dht) == 0 && --timeout) {}
    if (timeout == 0) { __enable_irq(); dht->present = false; return false; }
    DELAY_US(80);

    /* ── 3. 读取 40 bits ─────────────────────────────────── */
    for (int byte = 0; byte < 5; byte++) {
        for (int bit = 0; bit < 8; bit++) {
            /* 等待 bit 起始低电平 (50µs) */
            timeout = 1000;
            while (_pin_read(dht) == 0 && --timeout) {}

            /* 等待高电平开始, 延时 40µs 后采样 */
            DELAY_US(40);

            /* 采样: 如果还是高 → bit=1, 否则 bit=0 */
            uint8_t val = _pin_read(dht);
            data[byte] <<= 1;
            if (val) data[byte] |= 1;

            /* 等待高电平结束 (70µs for bit 1) */
            timeout = 1000;
            while (_pin_read(dht) == 1 && --timeout) {}
        }
    }

    __enable_irq();

    /* ── 4. 校验 ─────────────────────────────────────────── */
    uint8_t checksum = (uint8_t)(data[0] + data[1] + data[2] + data[3]);
    if (checksum != data[4]) {
        dht->present = false;
        return false;
    }

    dht->humidity = data[0];  /* 湿度整数 (DHT11 小数始终为 0) */
    dht->temp_c   = data[2];  /* 温度整数 */
    dht->present  = true;
    return true;
}

uint8_t dht11_get_temp(const DHT11 *dht)     { return dht->temp_c; }
uint8_t dht11_get_humidity(const DHT11 *dht) { return dht->humidity; }
bool    dht11_is_present(const DHT11 *dht)   { return dht->present; }
