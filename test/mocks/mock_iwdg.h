/**
 * @file    mock_iwdg.h
 * @brief   IWDG Mock — host 测试用, 通过 vtable 注入替换真实硬件操作
 */
#ifndef MOCK_IWDG_H
#define MOCK_IWDG_H

#include <stdint.h>
#include <stdbool.h>
#include "iwdg.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool      initialized;
    uint8_t   prescaler;
    uint16_t  reload;
    int       feed_count;
    uint32_t  timeout_ms;
} MockIwdgState;

/** @brief 重置 mock 状态 (每个测试开始前调用) */
void mock_iwdg_reset(MockIwdgState *s);

/** @brief 注入 mock vtable — 替换对象的 vtable 指针并绑定状态 */
void mock_iwdg_inject(Iwdg *wdog, MockIwdgState *state);

#ifdef __cplusplus
}
#endif

#endif /* MOCK_IWDG_H */
