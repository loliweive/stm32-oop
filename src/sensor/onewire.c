/**
 * @file    onewire.c
 * @brief   1-Wire 总线位操作 — 直接寄存器访问 (最优时序)
 *
 * 使用 BSRR/BRR/IDR 直接操作 GPIO 寄存器,
 * 避免函数调用开销, 保证 1-Wire 严格时序。
 *
 * 每个 GPIO 操作只有 2-3 条 ARM 指令:
 *   STR  R, [BSRR]  → 引脚置高/置低
 *   LDR  R, [IDR]   → 读取引脚
 */
#include "onewire.h"
#include "stm32f103xb.h"

/* ── DWT 硬件周期计数器 µs 延时 (Cortex-M3, 绝对精确) ──── */
static void _dwt_delay_us(uint32_t us) {
    uint32_t t = *(volatile uint32_t*)0xE0001004 + us * 72;
    while ((int32_t)(*(volatile uint32_t*)0xE0001004 - t) < 0) {}
}
#define DELAY_US(us) do { \
    static int _dwt_init = 0; \
    if (!_dwt_init) { \
        *(volatile uint32_t*)0xE000EDFC |= (1<<24); /* CoreDebug DEMCR: TRCENA */ \
        *(volatile uint32_t*)0xE0001004 = 0;         /* DWT CYCCNT = 0 */ \
        *(volatile uint32_t*)0xE0001000 |= 1;         /* DWT CTRL: CYCCNTENA */ \
        _dwt_init = 1; \
    } \
    _dwt_delay_us(us); \
} while(0)

/* ── GPIO 寄存器访问 ──────────────────────────────────────────── */
static inline void _pin_output(OneWireBus *bus) {
    GPIO_Type *g = (GPIO_Type *)bus->port;
    /* CRL/CRH: 开漏输出, 50MHz */
    uint32_t pos = 0; uint16_t m = bus->pin; while (!(m & (1<<pos))) pos++;
    if (pos < 8) {
        g->CRL = (g->CRL & ~(0xFUL << (pos*4))) | (0x7UL << (pos*4)); /* CNF=01 MODE=11 */
    } else {
        g->CRH = (g->CRH & ~(0xFUL << ((pos-8)*4))) | (0x7UL << ((pos-8)*4));
    }
}

static inline void _pin_input(OneWireBus *bus) {
    GPIO_Type *g = (GPIO_Type *)bus->port;
    uint32_t pos = 0; uint16_t m = bus->pin; while (!(m & (1<<pos))) pos++;
    if (pos < 8) {
        g->CRL = (g->CRL & ~(0xFUL << (pos*4))) | (0x4UL << (pos*4)); /* CNF=01 MODE=00 */
    } else {
        g->CRH = (g->CRH & ~(0xFUL << ((pos-8)*4))) | (0x4UL << ((pos-8)*4));
    }
}

static inline void _pin_low(OneWireBus *bus) {
    ((GPIO_Type *)bus->port)->BRR = bus->pin;  /* 原子写 — 置低 */
}

static inline void _pin_high(OneWireBus *bus) {
    ((GPIO_Type *)bus->port)->BSRR = bus->pin; /* 原子写 — 置高 */
}

static inline uint8_t _pin_read(OneWireBus *bus) {
    return (((GPIO_Type *)bus->port)->IDR & bus->pin) ? 1 : 0;
}

/* ── 公开 API ──────────────────────────────────────────────────── */

void ow_init(OneWireBus *bus, void *port, uint16_t pin)
{
    bus->port  = port;
    bus->pin   = pin;
    bus->_init = 1;
}

bool ow_reset(OneWireBus *bus)
{
    uint8_t presence;

    __disable_irq();
    _pin_output(bus);               /* 开漏输出模式 */
    _pin_low(bus);                  /* 拉低 */
    DELAY_US(480);                   /* 复位脉冲 480µs */
    _pin_input(bus);               /* 释放总线 (浮空输入) */
    _pin_high(bus);                /* 空写 → 释放开漏 */
    DELAY_US(70);
    presence = _pin_read(bus);     /* 采样存在脉冲 */
    __enable_irq();
    DELAY_US(410);                   /* 等待时隙结束 */

    return (presence == 0);        /* 0 = 有设备拉低 */
}

void ow_write_bit(OneWireBus *bus, uint8_t bit)
{
    __disable_irq();
    _pin_output(bus);
    _pin_low(bus);
    DELAY_US(2);                     /* 拉低 2µs */

    if (bit) {
        _pin_input(bus);           /* 释放 → 上拉电阻拉高 */
        _pin_high(bus);
        DELAY_US(58);               /* 保持高 60µs */
    } else {
        DELAY_US(58);               /* 保持低 60µs */
        _pin_input(bus);
        _pin_high(bus);
    }
    __enable_irq();
    DELAY_US(1);
}

uint8_t ow_read_bit(OneWireBus *bus)
{
    uint8_t bit;

    __disable_irq();
    _pin_output(bus);
    _pin_low(bus);
    DELAY_US(2);                     /* 拉低 2µs */
    _pin_input(bus);               /* 释放 */
    _pin_high(bus);
    DELAY_US(8);                     /* 等待 8µs */
    bit = _pin_read(bus);          /* 采样! */
    __enable_irq();
    DELAY_US(50);                    /* 完成时隙 */

    return bit;
}

void ow_write_byte(OneWireBus *bus, uint8_t byte)
{
    for (int i = 0; i < 8; i++) {
        ow_write_bit(bus, byte & 0x01);
        byte >>= 1;
    }
}

uint8_t ow_read_byte(OneWireBus *bus)
{
    uint8_t byte = 0;
    for (int i = 0; i < 8; i++) {
        byte >>= 1;
        if (ow_read_bit(bus)) byte |= 0x80;
    }
    return byte;
}

/* Dallas CRC8: x^8 + x^5 + x^4 + 1 */
uint8_t ow_crc8(const uint8_t *data, int len)
{
    uint8_t crc = 0;
    while (len--) {
        uint8_t byte = *data++;
        for (int i = 0; i < 8; i++) {
            uint8_t mix = (crc ^ byte) & 0x01;
            crc >>= 1;
            if (mix) crc ^= 0x8C;
            byte >>= 1;
        }
    }
    return crc;
}

/* ROM 搜索 — 单设备直接用 READ ROM */
int ow_search_rom(OneWireBus *bus, uint8_t rom[][8], int max)
{
    int found = 0;
    if (max >= 1 && ow_reset(bus)) {
        ow_write_byte(bus, 0x33);  /* READ ROM */
        for (int i = 0; i < 8; i++) {
            rom[0][i] = ow_read_byte(bus);
        }
        if (ow_crc8(rom[0], 8) == 0) found = 1;
    }
    return found;
}
