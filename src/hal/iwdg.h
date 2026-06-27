/**
 * @file    iwdg.h
 * @brief   IWDG 独立看门狗驱动
 *
 * IWDG 使用独立的 40kHz LSI 振荡器, 不受系统时钟影响。
 * 配置后一旦使能就无法软件禁用 (硬件安全设计)。
 *
 * 超时计算: T = (RLR+1) * (4 * 2^PR) / 40000 秒
 *
 * 使用:
 *   iwdg_init(6, 156);  // PR=6, RLR=156 → ~4秒超时
 *   while(1) { iwdg_feed(); vTaskDelay(1000); }
 */

#ifndef IWDG_HAL_H
#define IWDG_HAL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化并启动 IWDG
 * @param pr 预分频器 (0-7): /4, /8, /16, /32, /64, /128, /256, /256
 * @param rlr 重装载值 (0-4095)
 *
 * 启动后无法停止, 必须在超时前周期性调用 iwdg_feed()。
 */
void iwdg_init(uint8_t pr, uint16_t rlr);

/** @brief 喂狗 — 重置计数器, 防止复位 */
void iwdg_feed(void);

#ifdef __cplusplus
}
#endif

#endif /* IWDG_HAL_H */
