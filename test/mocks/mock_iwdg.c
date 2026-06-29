/**
 * @file    mock_iwdg.c
 * @brief   IWDG Mock 实现 — 记录调用到 MockIwdgState, 不操作真实硬件
 */
#include "mock_iwdg.h"
#include <string.h>

/* 单实例 active state (与 mock_gpio.c / mock_timer.c 模式一致) */
static MockIwdgState *active = NULL;

/* ── Mock vtable 函数 ───────────────────────────────────────── */

static void _mock_init(Iwdg *self, uint8_t prescaler, uint16_t reload)
{
    (void)self;
    if (!active) return;

    /* 幂等守卫: 已初始化则忽略 */
    if (active->initialized) return;

    active->prescaler   = prescaler;
    active->reload      = reload;
    active->initialized = true;

    /* 计算超时: T_ms = (RLR+1) * (4 * 2^PR) / 40 */
    uint32_t divider = (uint32_t)4 << prescaler;
    uint32_t ticks   = (uint32_t)reload + 1;
    active->timeout_ms = (ticks * divider) / 40;
}

static void _mock_feed(Iwdg *self)
{
    (void)self;
    if (!active) return;

    /* 未初始化时喂狗无效 */
    if (!active->initialized) return;

    active->feed_count++;
}

static uint32_t _mock_get_timeout_ms(const Iwdg *self)
{
    (void)self;
    return active ? active->timeout_ms : 0;
}

static uint8_t _mock_is_enabled(const Iwdg *self)
{
    (void)self;
    return active ? (uint8_t)active->initialized : 0;
}

/* ── Mock vtable 实例 ───────────────────────────────────────── */

static const IwdgVtable _mock_iwdg_vtable = {
    .init           = _mock_init,
    .feed           = _mock_feed,
    .get_timeout_ms = _mock_get_timeout_ms,
    .is_enabled     = _mock_is_enabled,
};

/* ── 公共接口 ────────────────────────────────────────────────── */

void mock_iwdg_reset(MockIwdgState *s)
{
    memset(s, 0, sizeof(*s));
}

void mock_iwdg_inject(Iwdg *wdog, MockIwdgState *state)
{
    wdog->vtable = &_mock_iwdg_vtable;
    active = state;
}
