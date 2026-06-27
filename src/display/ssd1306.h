/**
 * @file    ssd1306.h
 * @brief   SSD1306 OLED 128x64 I2C 驱动 — OOP in C
 *
 * 实现 OledDisplay 抽象接口。继承自 OledDisplay base。
 *
 * ── 类继承 ────────────────────────────────────────────
 *
 *   OledDisplay (abstract, oled.h)
 *     ▲
 *     │ "继承" (第一个成员 = OledDisplay base)
 *     │
 *   SSD1306 (ssd1306.h/c)
 *     framebuf[8][128] — 1024 bytes
 *     I2C 通信 — 使用项目 HAL (i2c_write_raw)
 *
 * ── 接线 ──────────────────────────────────────────────
 *   SCL → PB6 (I2C1_SCL)
 *   SDA → PB7 (I2C1_SDA)
 *   VCC → 3.3V, GND → GND
 *
 * ── I2C 地址 ──────────────────────────────────────────
 *   0x3C (SA0=0), 或 0x3D (SA0=1)
 *
 * ── 典型使用 ──────────────────────────────────────────
 *
 *   SSD1306 oled;
 *   ssd1306_ctor(&oled, I2C1, 0x3C);
 *   OledDisplay *d = &oled.base;
 *
 *   oled_init(d);
 *   oled_show_string(d, 0, 0, "Hello!", OLED_FONT_8X16);
 *   oled_flush(d);
 *
 * ── 移植自 ────────────────────────────────────────────
 *   江协科技 OLED 驱动 V2.0 (2024.10.20)
 *   重构为 OOP vtable 模式
 */

#ifndef SSD1306_H
#define SSD1306_H

#include "oled.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── 显示尺寸 ──────────────────────────────────────────── */
#define SSD1306_WIDTH   128
#define SSD1306_HEIGHT  64
#define SSD1306_PAGES   (SSD1306_HEIGHT / 8)

/**
 * @brief SSD1306 具体驱动
 *
 * 第一个成员必须是 OledDisplay base (类似 C++ 继承)。
 * framebuf 在对象内部，非全局变量，支持多实例。
 */
typedef struct {
    OledDisplay base;                           /**< 基类 (vtable) */
    void       *i2c;                            /**< I2C_Type* */
    uint8_t     addr;                           /**< I2C 从机地址 */
    uint8_t     framebuf[SSD1306_PAGES][SSD1306_WIDTH]; /**< 显存 */
} SSD1306;

/**
 * @brief SSD1306 构造函数
 *
 * 初始化对象，绑定 SSD1306 vtable。
 * 调用后对象可用，但需先 oled_init() 初始化硬件。
 *
 * @param self 未初始化的 SSD1306 对象
 * @param i2c  I2C 外设基地址 (I2C1, I2C2)
 * @param addr I2C 从机地址 (通常 0x3C)
 */
void ssd1306_ctor(SSD1306 *self, void *i2c, uint8_t addr);

#ifdef __cplusplus
}
#endif

#endif /* SSD1306_H */
