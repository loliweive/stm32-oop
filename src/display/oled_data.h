/**
 * @file    oled_data.h
 * @brief   OLED 字模数据声明
 *
 * 包含 ASCII 8x16、6x8 和中文 16x16 字模。
 * 移植自江协科技 OLED 驱动。
 */

#ifndef OLED_DATA_H
#define OLED_DATA_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── 字符集选择 (只可定义一个) ──────────────────────────── */
#define OLED_CHARSET_UTF8

/* ── 中文字模结构 ──────────────────────────────────────── */
#ifdef OLED_CHARSET_UTF8
typedef struct {
    char     index[5];   /**< UTF-8 汉字索引 (最多4字节+终止符) */
    uint8_t  data[32];   /**< 16x16 字模数据 */
} ChineseCell_t;
#else
typedef struct {
    char     index[3];   /**< GB2312 汉字索引 (2字节+终止符) */
    uint8_t  data[32];   /**< 16x16 字模数据 */
} ChineseCell_t;
#endif

/* ── ASCII 字模 ─────────────────────────────────────────── */

/** 宽8像素，高16像素 ASCII 字模 (95 字符, 空格~波浪号) */
extern const uint8_t OLED_F8x16[][16];

/** 宽6像素，高8像素 ASCII 字模 (95 字符) */
extern const uint8_t OLED_F6x8[][6];

/* ── 中文字模 ──────────────────────────────────────────── */

/** 宽16像素，高16像素 中文字模 */
extern const ChineseCell_t OLED_CF16x16[];

/* ── 图像数据 ──────────────────────────────────────────── */

/** 测试图像：二极管符号，宽16像素，高16像素 */
extern const uint8_t Diode[];

#ifdef __cplusplus
}
#endif

#endif /* OLED_DATA_H */
