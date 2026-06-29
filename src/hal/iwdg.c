/**
 * @file    iwdg.c
 * @brief   IWDG 独立看门狗 — OOP vtable 实现 (STM32F1 寄存器级)
 */

#include "iwdg.h"

/* 目标编译: 使用真实 stm32f103xb.h; host 编译: 使用 test/mocks/ 中的 stub */
#include "stm32f103xb.h"

/* ── IWDG 密钥常量 ─────────────────────────────────────────── */
#define IWDG_KEY_UNLOCK  0x5555U  /* 解锁 PR/RLR 写保护 */
#define IWDG_KEY_FEED    0xAAAAU  /* 重装载计数器 (喂狗) */
#define IWDG_KEY_START   0xCCCCU  /* 启动看门狗 (不可逆) */

/* ── 生产 vtable 函数 ──────────────────────────────────────── */

static void _iwdg_init(Iwdg *self, uint8_t prescaler, uint16_t reload)
{
    /* 幂等守卫: IWDG 启动后不可重新配置 */
    if (self->_init) return;

    IWDG_TypeDef *iwdg = (IWDG_TypeDef *)self->iwdg;

    /* 1. 开启 LSI (IWDG 时钟源, 必须先启动) */
    RCC->CSR |= RCC_CSR_LSION;
    uint32_t to = 100000;
    while (!(RCC->CSR & RCC_CSR_LSIRDY) && --to) {}

    /* 2. 解锁 PR 和 RLR 寄存器 */
    iwdg->KR = IWDG_KEY_UNLOCK;
    iwdg->PR = prescaler & 0x07;
    iwdg->RLR = reload & 0x0FFF;

    /* 3. 等待寄存器更新完成 (硬件同步) */
    while (iwdg->SR & IWDG_SR_PVU) {}
    while (iwdg->SR & IWDG_SR_RVU) {}

    /* 4. 启动 IWDG (不可逆 — 写 0xCCCC 后硬件永久使能) */
    iwdg->KR = IWDG_KEY_START;

    self->prescaler = prescaler;
    self->reload    = reload;
    self->_init     = 1;
}

static void _iwdg_feed(Iwdg *self)
{
    /* 安全守卫: 未启动时喂狗无意义 */
    if (!self->_init) return;
    IWDG_TypeDef *iwdg = (IWDG_TypeDef *)self->iwdg;
    iwdg->KR = IWDG_KEY_FEED;
}

static uint32_t _iwdg_get_timeout_ms(const Iwdg *self)
{
    /*
     * T = (RLR+1) * (4 * 2^PR) / 40000  秒
     *   = (RLR+1) * (4 * 2^PR) / 40     毫秒
     */
    uint32_t divider = (uint32_t)4 << self->prescaler;  /* 4 * 2^PR */
    uint32_t ticks   = (uint32_t)self->reload + 1;
    return (ticks * divider) / 40;
}

static uint8_t _iwdg_is_enabled(const Iwdg *self)
{
    return self->_init;
}

/* ── Vtable 实例 ───────────────────────────────────────────── */

static const IwdgVtable _iwdg_vtable = {
    .init           = _iwdg_init,
    .feed           = _iwdg_feed,
    .get_timeout_ms = _iwdg_get_timeout_ms,
    .is_enabled     = _iwdg_is_enabled,
};

/* ── 构造函数 ──────────────────────────────────────────────── */

void Iwdg_ctor(Iwdg *self, void *iwdg_base)
{
    self->iwdg      = iwdg_base;
    self->vtable    = &_iwdg_vtable;
    self->prescaler = 0;
    self->reload    = 0;
    self->_init     = 0;
}
