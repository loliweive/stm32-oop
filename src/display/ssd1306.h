/**
 * @file    ssd1306.h
 * @brief   SSD1306 OLED 128x64 I2C 驱动
 *
 * 接线:
 *   SCL → PB6 (I2C1)
 *   SDA → PB7 (I2C1)
 *   VCC → 3.3V
 *   GND → GND
 *
 * I2C 地址: 0x3C
 * 分辨率:  128x64 像素
 * 帧缓冲:  1024 bytes (128*64/8)
 */

#ifndef SSD1306_H
#define SSD1306_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── 显示尺寸 ──────────────────────────────────────────── */
#define SSD1306_WIDTH   128
#define SSD1306_HEIGHT  64
#define SSD1306_PAGES   (SSD1306_HEIGHT / 8)  /* 8 pages × 8 rows */

/* ── OLED 对象 ──────────────────────────────────────────── */
typedef struct {
    void    *i2c;           /**< I2C 外设 (I2C1) */
    uint8_t  addr;          /**< I2C 地址 (0x3C) */
    uint8_t  framebuf[SSD1306_WIDTH * SSD1306_PAGES]; /**< 帧缓冲 */
} SSD1306;

/**
 * @brief 初始化 OLED
 * @param oled 未初始化对象
 * @param i2c  I2C 外设 (I2C1)
 * @param addr I2C 地址 (0x3C)
 */
void ssd1306_init(SSD1306 *oled, void *i2c, uint8_t addr);

/** @brief 清空帧缓冲 */
void ssd1306_clear(SSD1306 *oled);

/** @brief 将帧缓冲刷新到 OLED (发送全部数据) */
void ssd1306_flush(SSD1306 *oled);

/** @brief 设置像素 (0,0 = 左上角) */
void ssd1306_pixel(SSD1306 *oled, uint8_t x, uint8_t y, bool on);

/** @brief 绘制水平线 */
void ssd1306_hline(SSD1306 *oled, uint8_t x, uint8_t y, uint8_t w);

/** @brief 绘制填充矩形 */
void ssd1306_fill_rect(SSD1306 *oled, uint8_t x, uint8_t y, uint8_t w, uint8_t h, bool on);

/**
 * @brief 在 (x, page) 绘制 5×7 字符
 * @param x    列 (0-127)
 * @param page 页 (0-7, 每页 8 像素行)
 * @param c    字符
 */
void ssd1306_char(SSD1306 *oled, uint8_t x, uint8_t page, char c);

/**
 * @brief 绘制字符串 (自动换页)
 * @param x    起始列
 * @param page 起始页
 * @param str  字符串
 */
void ssd1306_text(SSD1306 *oled, uint8_t x, uint8_t page, const char *str);

/**
 * @brief 格式化并显示传感器数据
 * @param sensor_name 传感器名称
 * @param line1       第 1 行文本
 * @param line2       第 2 行文本
 */
void ssd1306_show_sensor(SSD1306 *oled, const char *sensor_name,
                         const char *line1, const char *line2);

#ifdef __cplusplus
}
#endif

#endif
