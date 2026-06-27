/**
 * @file    oled.h
 * @brief   OLED 显示抽象接口 — OOP in C (vtable 多态)
 *
 * 所有 OLED 驱动实现此接口，应用层通过 OledDisplay* 统一操作。
 *
 * ── 类继承结构 ──────────────────────────────────────────
 *
 *        OledDisplay (abstract)
 *        │  vtable: init(), clear(), flush(), draw_*(), show_*()
 *        │
 *        SSD1306 (I2C, 128x64)
 *
 * ── 使用方法 ────────────────────────────────────────────
 *
 *   SSD1306 oled;
 *   ssd1306_ctor(&oled, I2C1, 0x3C);
 *   OledDisplay *d = &oled.base;
 *
 *   oled_init(d);
 *   oled_show_string(d, 0, 0, "Hello", OLED_FONT_8X16);
 *   oled_flush(d);
 *
 * ── 坐标系统 ────────────────────────────────────────────
 *   左上角为 (0, 0)，X 向右 (0~127)，Y 向下 (0~63)
 *   纵向 8 点为一页 (page)，高位在下
 *
 * ── 参考 ────────────────────────────────────────────────
 *   移植自江协科技 OLED 驱动 (V2.0, 2024.10.20)
 *   重构为 OOP vtable 模式，使用项目 I2C HAL
 */

#ifndef OLED_H
#define OLED_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── 字体大小常量 ──────────────────────────────────────── */
#define OLED_FONT_6X8   6   /**< 宽6像素，高8像素 */
#define OLED_FONT_8X16  8   /**< 宽8像素，高16像素 */

/* ── 填充标志 ──────────────────────────────────────────── */
#define OLED_UNFILLED   0
#define OLED_FILLED     1

/* ── 前向声明 ──────────────────────────────────────────── */
typedef struct OledDisplay OledDisplay;

/**
 * @brief OLED 显示虚函数表
 *
 * 定义所有 OLED 显示操作。具体驱动实现此 vtable。
 */
typedef struct {
    /* ── 硬件控制 ──────────────────────────────────── */
    void (*init)(OledDisplay *self);
    void (*clear)(OledDisplay *self);
    void (*flush)(OledDisplay *self);
    void (*flush_area)(OledDisplay *self, int16_t x, int16_t y,
                       uint8_t width, uint8_t height);
    void (*reverse)(OledDisplay *self);
    void (*reverse_area)(OledDisplay *self, int16_t x, int16_t y,
                         uint8_t width, uint8_t height);

    /* ── 像素操作 ──────────────────────────────────── */
    void    (*draw_point)(OledDisplay *self, int16_t x, int16_t y);
    uint8_t (*get_point)(OledDisplay *self, int16_t x, int16_t y);

    /* ── 几何绘图 ──────────────────────────────────── */
    void (*draw_line)(OledDisplay *self,
                      int16_t x0, int16_t y0, int16_t x1, int16_t y1);
    void (*draw_rect)(OledDisplay *self, int16_t x, int16_t y,
                      uint8_t width, uint8_t height, uint8_t filled);
    void (*draw_triangle)(OledDisplay *self,
                          int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                          int16_t x2, int16_t y2, uint8_t filled);
    void (*draw_circle)(OledDisplay *self, int16_t x, int16_t y,
                        uint8_t radius, uint8_t filled);
    void (*draw_ellipse)(OledDisplay *self, int16_t x, int16_t y,
                         uint8_t a, uint8_t b, uint8_t filled);
    void (*draw_arc)(OledDisplay *self, int16_t x, int16_t y,
                     uint8_t radius, int16_t start_angle, int16_t end_angle,
                     uint8_t filled);

    /* ── 文本显示 ──────────────────────────────────── */
    void (*show_char)(OledDisplay *self, int16_t x, int16_t y,
                      char ch, uint8_t font_size);
    void (*show_string)(OledDisplay *self, int16_t x, int16_t y,
                        const char *str, uint8_t font_size);
    void (*show_num)(OledDisplay *self, int16_t x, int16_t y,
                     uint32_t num, uint8_t length, uint8_t font_size);
    void (*show_signed)(OledDisplay *self, int16_t x, int16_t y,
                        int32_t num, uint8_t length, uint8_t font_size);
    void (*show_hex)(OledDisplay *self, int16_t x, int16_t y,
                     uint32_t num, uint8_t length, uint8_t font_size);
    void (*show_bin)(OledDisplay *self, int16_t x, int16_t y,
                     uint32_t num, uint8_t length, uint8_t font_size);
    void (*show_float)(OledDisplay *self, int16_t x, int16_t y,
                       double num, uint8_t int_len, uint8_t frac_len,
                       uint8_t font_size);
    void (*show_image)(OledDisplay *self, int16_t x, int16_t y,
                       uint8_t width, uint8_t height, const uint8_t *image);

    /* ── 格式化输出 ────────────────────────────────── */
    void (*printf)(OledDisplay *self, int16_t x, int16_t y,
                   uint8_t font_size, const char *fmt, ...);
} OledVtable;

/**
 * @brief OLED 显示基类
 *
 * 每个具体 OLED 驱动的第一个成员必须是 OledDisplay base。
 * 类似 C++ 继承: struct SSD1306 : OledDisplay { ... };
 */
struct OledDisplay {
    const OledVtable *vtable;
};

/* ── 内联调度器 (类似 HAL 层的 gpio_set/gpio_get) ──────── */

static inline void oled_init(OledDisplay *self) {
    self->vtable->init(self);
}

static inline void oled_clear(OledDisplay *self) {
    self->vtable->clear(self);
}

static inline void oled_flush(OledDisplay *self) {
    self->vtable->flush(self);
}

static inline void oled_flush_area(OledDisplay *self, int16_t x, int16_t y,
                                   uint8_t w, uint8_t h) {
    self->vtable->flush_area(self, x, y, w, h);
}

static inline void oled_reverse(OledDisplay *self) {
    self->vtable->reverse(self);
}

static inline void oled_reverse_area(OledDisplay *self, int16_t x, int16_t y,
                                     uint8_t w, uint8_t h) {
    self->vtable->reverse_area(self, x, y, w, h);
}

static inline void oled_draw_point(OledDisplay *self, int16_t x, int16_t y) {
    self->vtable->draw_point(self, x, y);
}

static inline uint8_t oled_get_point(OledDisplay *self, int16_t x, int16_t y) {
    return self->vtable->get_point(self, x, y);
}

static inline void oled_draw_line(OledDisplay *self,
                                  int16_t x0, int16_t y0,
                                  int16_t x1, int16_t y1) {
    self->vtable->draw_line(self, x0, y0, x1, y1);
}

static inline void oled_draw_rect(OledDisplay *self, int16_t x, int16_t y,
                                  uint8_t w, uint8_t h, uint8_t filled) {
    self->vtable->draw_rect(self, x, y, w, h, filled);
}

static inline void oled_draw_triangle(OledDisplay *self,
                                      int16_t x0, int16_t y0,
                                      int16_t x1, int16_t y1,
                                      int16_t x2, int16_t y2,
                                      uint8_t filled) {
    self->vtable->draw_triangle(self, x0, y0, x1, y1, x2, y2, filled);
}

static inline void oled_draw_circle(OledDisplay *self, int16_t x, int16_t y,
                                    uint8_t r, uint8_t filled) {
    self->vtable->draw_circle(self, x, y, r, filled);
}

static inline void oled_draw_ellipse(OledDisplay *self, int16_t x, int16_t y,
                                     uint8_t a, uint8_t b, uint8_t filled) {
    self->vtable->draw_ellipse(self, x, y, a, b, filled);
}

static inline void oled_draw_arc(OledDisplay *self, int16_t x, int16_t y,
                                 uint8_t r, int16_t start_angle,
                                 int16_t end_angle, uint8_t filled) {
    self->vtable->draw_arc(self, x, y, r, start_angle, end_angle, filled);
}

static inline void oled_show_char(OledDisplay *self, int16_t x, int16_t y,
                                  char ch, uint8_t font_size) {
    self->vtable->show_char(self, x, y, ch, font_size);
}

static inline void oled_show_string(OledDisplay *self, int16_t x, int16_t y,
                                    const char *str, uint8_t font_size) {
    self->vtable->show_string(self, x, y, str, font_size);
}

static inline void oled_show_num(OledDisplay *self, int16_t x, int16_t y,
                                 uint32_t num, uint8_t len, uint8_t font_size) {
    self->vtable->show_num(self, x, y, num, len, font_size);
}

static inline void oled_show_signed(OledDisplay *self, int16_t x, int16_t y,
                                    int32_t num, uint8_t len, uint8_t font_size) {
    self->vtable->show_signed(self, x, y, num, len, font_size);
}

static inline void oled_show_hex(OledDisplay *self, int16_t x, int16_t y,
                                 uint32_t num, uint8_t len, uint8_t font_size) {
    self->vtable->show_hex(self, x, y, num, len, font_size);
}

static inline void oled_show_bin(OledDisplay *self, int16_t x, int16_t y,
                                 uint32_t num, uint8_t len, uint8_t font_size) {
    self->vtable->show_bin(self, x, y, num, len, font_size);
}

static inline void oled_show_float(OledDisplay *self, int16_t x, int16_t y,
                                   double num, uint8_t int_len,
                                   uint8_t frac_len, uint8_t font_size) {
    self->vtable->show_float(self, x, y, num, int_len, frac_len, font_size);
}

static inline void oled_show_image(OledDisplay *self, int16_t x, int16_t y,
                                   uint8_t w, uint8_t h, const uint8_t *img) {
    self->vtable->show_image(self, x, y, w, h, img);
}

/* oled_printf is NOT inline — it uses varargs, declared in ssd1306.c */
void oled_printf(OledDisplay *self, int16_t x, int16_t y,
                 uint8_t font_size, const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif /* OLED_H */
