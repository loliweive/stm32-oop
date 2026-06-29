/**
 * @file    iwdg.h
 * @brief   IWDG 独立看门狗 OOP vtable 驱动
 *
 * IWDG 使用独立的 40kHz LSI 振荡器, 不受系统时钟影响。
 * 启动后无法软件禁用 (硬件安全设计)。
 *
 * 超时计算: T = (RLR+1) * (4 * 2^PR) / 40000 秒
 *
 * 使用:
 *   Iwdg wdog;
 *   Iwdg_ctor(&wdog, IWDG);
 *   iwdg_init(&wdog, 6, 0xFFF);   // PR=6/256, RLR=4095 → ~10-26s
 *   while(1) { iwdg_feed(&wdog); vTaskDelay(1000); }
 */

#ifndef IWDG_HAL_H
#define IWDG_HAL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── 前置声明 ──────────────────────────────────────────────── */
typedef struct Iwdg Iwdg;

/* ── Vtable (虚函数表) ──────────────────────────────────────── */
typedef struct {
    void     (*init)(Iwdg *self, uint8_t prescaler, uint16_t reload);
    void     (*feed)(Iwdg *self);
    uint32_t (*get_timeout_ms)(const Iwdg *self);
    uint8_t  (*is_enabled)(const Iwdg *self);
} IwdgVtable;

/* ── Context 结构体 ─────────────────────────────────────────── */
struct Iwdg {
    void             *iwdg;       /* IWDG_TypeDef* — void* 避免头文件依赖 */
    const IwdgVtable *vtable;
    uint16_t          reload;     /* 最后一次 init 的 reload 值 */
    uint8_t           prescaler;  /* 最后一次 init 的 prescaler 值 */
    uint8_t           _init;      /* 私有: 0=未启动, 1=已启动 */
};

/* ── 构造函数 ──────────────────────────────────────────────── */
void Iwdg_ctor(Iwdg *self, void *iwdg_base);

/* ── 公共 API (inline 分发到 vtable) ────────────────────────── */

/** @brief 初始化并启动 IWDG (一次性, 启动后不可停)
 *  @param prescaler 预分频器 (0-7): /4, /8, /16, /32, /64, /128, /256, /256
 *  @param reload    重装载值 (0-4095) */
static inline void iwdg_init(Iwdg *self, uint8_t prescaler, uint16_t reload) {
    self->vtable->init(self, prescaler, reload);
}

/** @brief 喂狗 — 重置计数器, 防止复位 */
static inline void iwdg_feed(Iwdg *self) {
    self->vtable->feed(self);
}

/** @brief 获取当前配置的超时时间 (毫秒) */
static inline uint32_t iwdg_get_timeout_ms(const Iwdg *self) {
    return self->vtable->get_timeout_ms(self);
}

/** @brief 查询 IWDG 是否已启动 */
static inline uint8_t iwdg_is_enabled(const Iwdg *self) {
    return self->vtable->is_enabled(self);
}

#ifdef __cplusplus
}
#endif

#endif /* IWDG_HAL_H */
