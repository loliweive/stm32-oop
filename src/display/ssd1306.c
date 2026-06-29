/**
 * @file    ssd1306.c
 * @brief   SSD1306 OLED 128x64 I2C 驱动实现 — OOP vtable 模式
 *
 * 实现 OledDisplay 接口的所有方法：
 *   - 硬件控制：init, clear, flush, flush_area, reverse
 *   - 像素操作：draw_point, get_point
 *   - 几何绘图：draw_line, draw_rect, draw_triangle,
 *               draw_circle, draw_ellipse, draw_arc
 *   - 文本显示：show_char, show_string, show_num, show_hex, show_bin,
 *               show_float, show_image, printf
 *
 * 移植自江协科技 OLED 驱动 V2.0，重构为 OOP vtable 模式。
 */

#include "ssd1306.h"
#include "oled_data.h"
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <stdarg.h>

/* ── 目标硬件 I2C — HAL bulk transfer ────────────────── */
#ifdef STM32F103xB
#include "stm32f1xx_hal.h"

static bool i2c_write_raw(void *i2c, uint8_t addr, const uint8_t *data, size_t len)
{
    I2C_HandleTypeDef hi2c;
    hi2c.Instance = (I2C_TypeDef *)i2c;
    hi2c.State = HAL_I2C_STATE_READY;
    return HAL_I2C_Master_Transmit(&hi2c, (uint16_t)(addr << 1),
                                   (uint8_t *)data, (uint16_t)len, 100) == HAL_OK;
}

#else /* HOST_TEST — mock 实现 */

#include <stdbool.h>
static bool i2c_write_raw(void *i2c, uint8_t addr, const uint8_t *data, size_t len)
{
    (void)i2c; (void)addr; (void)data; (void)len;
    return true; /* host 测试中无真实 I2C */
}
#endif /* STM32F103xB */

/* ── 前向声明 (vtable 函数) ──────────────────────────── */
static void _ssd1306_init(OledDisplay *self);
static void _ssd1306_clear(OledDisplay *self);
static void _ssd1306_flush(OledDisplay *self);
static void _ssd1306_flush_area(OledDisplay *self, int16_t x, int16_t y,
                                uint8_t w, uint8_t h);
static void _ssd1306_reverse(OledDisplay *self);
static void _ssd1306_reverse_area(OledDisplay *self, int16_t x, int16_t y,
                                  uint8_t w, uint8_t h);
static void _ssd1306_draw_point(OledDisplay *self, int16_t x, int16_t y);
static uint8_t _ssd1306_get_point(OledDisplay *self, int16_t x, int16_t y);
static void _ssd1306_draw_line(OledDisplay *self, int16_t x0, int16_t y0,
                               int16_t x1, int16_t y1);
static void _ssd1306_draw_rect(OledDisplay *self, int16_t x, int16_t y,
                               uint8_t w, uint8_t h, uint8_t filled);
static void _ssd1306_draw_triangle(OledDisplay *self,
                                   int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                                   int16_t x2, int16_t y2, uint8_t filled);
static void _ssd1306_draw_circle(OledDisplay *self, int16_t x, int16_t y,
                                 uint8_t r, uint8_t filled);
static void _ssd1306_draw_ellipse(OledDisplay *self, int16_t x, int16_t y,
                                  uint8_t a, uint8_t b, uint8_t filled);
static void _ssd1306_draw_arc(OledDisplay *self, int16_t x, int16_t y,
                              uint8_t r, int16_t start_angle, int16_t end_angle,
                              uint8_t filled);
static void _ssd1306_show_char(OledDisplay *self, int16_t x, int16_t y,
                               char ch, uint8_t font_size);
static void _ssd1306_show_string(OledDisplay *self, int16_t x, int16_t y,
                                 const char *str, uint8_t font_size);
static void _ssd1306_show_num(OledDisplay *self, int16_t x, int16_t y,
                              uint32_t num, uint8_t len, uint8_t font_size);
static void _ssd1306_show_signed(OledDisplay *self, int16_t x, int16_t y,
                                 int32_t num, uint8_t len, uint8_t font_size);
static void _ssd1306_show_hex(OledDisplay *self, int16_t x, int16_t y,
                              uint32_t num, uint8_t len, uint8_t font_size);
static void _ssd1306_show_bin(OledDisplay *self, int16_t x, int16_t y,
                              uint32_t num, uint8_t len, uint8_t font_size);
static void _ssd1306_show_float(OledDisplay *self, int16_t x, int16_t y,
                                double num, uint8_t int_len, uint8_t frac_len,
                                uint8_t font_size);
static void _ssd1306_show_image(OledDisplay *self, int16_t x, int16_t y,
                                uint8_t w, uint8_t h, const uint8_t *img);

/* ── 工具函数 ────────────────────────────────────────── */

/** @brief 次方函数 (内部使用) */
static uint32_t oled_pow(uint32_t x, uint32_t y)
{
    uint32_t result = 1;
    while (y--) result *= x;
    return result;
}

/**
 * @brief 判断指定点是否在指定多边形内部
 * @ref   W. Randolph Franklin, https://wrfranklin.org/Research/Short_Notes/pnpoly.html
 */
static uint8_t oled_pnpoly(uint8_t nvert, int16_t *vertx, int16_t *verty,
                           int16_t testx, int16_t testy)
{
    int16_t i, j, c = 0;
    for (i = 0, j = nvert - 1; i < nvert; j = i++) {
        if (((verty[i] > testy) != (verty[j] > testy)) &&
            (testx < (vertx[j] - vertx[i]) * (testy - verty[i]) /
                      (verty[j] - verty[i]) + vertx[i])) {
            c = !c;
        }
    }
    return c;
}

/**
 * @brief 判断指定点是否在指定角度内部
 * @param start_angle 起始角度 (-180~180), 水平向右=0, 顺时针旋转
 * @param end_angle   终止角度 (-180~180)
 */
static uint8_t oled_is_in_angle(int16_t x, int16_t y,
                                int16_t start_angle, int16_t end_angle)
{
    int16_t point_angle = atan2(y, x) / 3.14 * 180;
    if (start_angle < end_angle) {
        return (point_angle >= start_angle && point_angle <= end_angle) ? 1 : 0;
    } else {
        return (point_angle >= start_angle || point_angle <= end_angle) ? 1 : 0;
    }
}

/* ── 辅助: 获取 SSD1306 指针 ─────────────────────────── */
static inline SSD1306 *_ssd(OledDisplay *self) {
    return (SSD1306 *)self;
}

/* ── 低层 I2C 发送 ───────────────────────────────────── */

/** @brief 发送命令字节 */
static void _send_cmd(SSD1306 *oled, uint8_t cmd)
{
    uint8_t buf[2] = {0x00, cmd}; /* Co=0, D/C#=0 → 命令 */
    i2c_write_raw(oled->i2c, oled->addr, buf, 2);
}

/** @brief 发送数据块 (控制字节 0x40 = 数据模式) */
static void _send_data(SSD1306 *oled, const uint8_t *data, size_t len)
{
    for (size_t i = 0; i < len; i += 128) {
        size_t chunk = (len - i > 128) ? 128 : (len - i);
        uint8_t buf[129];
        buf[0] = 0x40; /* Co=0, D/C#=1 → 数据 */
        memcpy(buf + 1, data + i, chunk);
        i2c_write_raw(oled->i2c, oled->addr, buf, chunk + 1);
    }
}

/** @brief 设置光标位置 (页地址 + 列地址) */
static void _set_cursor(SSD1306 *oled, uint8_t page, uint8_t col)
{
    _send_cmd(oled, 0xB0 | page);                    /* 页地址 */
    _send_cmd(oled, 0x10 | ((col & 0xF0) >> 4));     /* 列地址高4位 */
    _send_cmd(oled, 0x00 | (col & 0x0F));             /* 列地址低4位 */
}

/* ═══════════════════════════════════════════════════════
 * vtable 实现
 * ═══════════════════════════════════════════════════════ */

/* ── 硬件控制 ────────────────────────────────────────── */

static void _ssd1306_init(OledDisplay *self)
{
    SSD1306 *oled = _ssd(self);

    /* SSD1306 初始化序列 (Display OFF 状态下配置) */
    uint8_t init_cmds[] = {
        0xAE,        /* Display OFF */
        0x20, 0x02,  /* 内存寻址模式 → Page Addressing */
        0xD5, 0x80,  /* 设置显示时钟分频比/振荡器频率 */
        0xA8, 0x3F,  /* 设置多路复用率 (64) */
        0xD3, 0x00,  /* 设置显示偏移 */
        0x40,        /* 设置显示开始行 */
        0xA1,        /* 设置左右方向 (正常) */
        0xC8,        /* 设置上下方向 (正常) */
        0xDA, 0x12,  /* 设置 COM 引脚硬件配置 */
        0x81, 0xCF,  /* 设置对比度 */
        0xD9, 0xF1,  /* 设置预充电周期 */
        0xDB, 0x30,  /* 设置 VCOMH 取消选择级别 */
        0xA4,        /* 整个显示打开/关闭 (正常) */
        0xA6,        /* 设置正常/反色显示 (正常) */
        0x8D, 0x14,  /* 设置充电泵 (启用) */
    };
    for (size_t i = 0; i < sizeof(init_cmds); i++) {
        _send_cmd(oled, init_cmds[i]);
    }

    /* 先清空 GRAM (显示仍为 OFF, 无闪烁) */
    memset(oled->framebuf, 0, sizeof(oled->framebuf));
    _ssd1306_flush(self);

    /* 最后开显示 — 此时 GRAM 已全零, 纯黑屏 */
    _send_cmd(oled, 0xAF);
}

static void _ssd1306_clear(OledDisplay *self)
{
    SSD1306 *oled = _ssd(self);
    memset(oled->framebuf, 0, sizeof(oled->framebuf));
}

static void _ssd1306_flush(OledDisplay *self)
{
    SSD1306 *oled = _ssd(self);
    for (uint8_t j = 0; j < SSD1306_PAGES; j++) {
        _set_cursor(oled, j, 0);
        _send_data(oled, oled->framebuf[j], SSD1306_WIDTH);
    }
}

static void _ssd1306_flush_area(OledDisplay *self, int16_t x, int16_t y,
                                uint8_t width, uint8_t height)
{
    SSD1306 *oled = _ssd(self);
    int16_t page, page1;

    page  = y / 8;
    page1 = (y + height - 1) / 8 + 1;
    if (y < 0) { page -= 1; page1 -= 1; }

    for (int16_t j = page; j < page1; j++) {
        if (x >= 0 && x <= 127 && j >= 0 && j <= 7) {
            _set_cursor(oled, (uint8_t)j, (uint8_t)x);
            _send_data(oled, &oled->framebuf[j][x], width);
        }
    }
}

static void _ssd1306_reverse(OledDisplay *self)
{
    SSD1306 *oled = _ssd(self);
    for (uint8_t j = 0; j < SSD1306_PAGES; j++) {
        for (uint8_t i = 0; i < SSD1306_WIDTH; i++) {
            oled->framebuf[j][i] ^= 0xFF;
        }
    }
}

static void _ssd1306_reverse_area(OledDisplay *self, int16_t x, int16_t y,
                                  uint8_t width, uint8_t height)
{
    SSD1306 *oled = _ssd(self);
    for (int16_t j = y; j < y + height; j++) {
        for (int16_t i = x; i < x + width; i++) {
            if (i >= 0 && i <= 127 && j >= 0 && j <= 63) {
                oled->framebuf[j / 8][i] ^= (0x01 << (j % 8));
            }
        }
    }
}

/* ── 像素操作 ────────────────────────────────────────── */

static void _ssd1306_draw_point(OledDisplay *self, int16_t x, int16_t y)
{
    SSD1306 *oled = _ssd(self);
    if (x >= 0 && x <= 127 && y >= 0 && y <= 63) {
        oled->framebuf[y / 8][x] |= (0x01 << (y % 8));
    }
}

static uint8_t _ssd1306_get_point(OledDisplay *self, int16_t x, int16_t y)
{
    SSD1306 *oled = _ssd(self);
    if (x >= 0 && x <= 127 && y >= 0 && y <= 63) {
        if (oled->framebuf[y / 8][x] & (0x01 << (y % 8))) return 1;
    }
    return 0;
}

/* ── 几何绘图 ────────────────────────────────────────── */

static void _ssd1306_draw_line(OledDisplay *self,
                               int16_t x0, int16_t y0,
                               int16_t x1, int16_t y1)
{
    int16_t x, y, dx, dy, d, incrE, incrNE, temp;
    uint8_t yflag = 0, xyflag = 0;

    if (y0 == y1) {
        /* 横线 */
        if (x0 > x1) { temp = x0; x0 = x1; x1 = temp; }
        for (x = x0; x <= x1; x++) _ssd1306_draw_point(self, x, y0);
        return;
    }
    if (x0 == x1) {
        /* 竖线 */
        if (y0 > y1) { temp = y0; y0 = y1; y1 = temp; }
        for (y = y0; y <= y1; y++) _ssd1306_draw_point(self, x0, y);
        return;
    }

    /* Bresenham 算法画斜线 */
    if (x0 > x1) {
        temp = x0; x0 = x1; x1 = temp;
        temp = y0; y0 = y1; y1 = temp;
    }

    if (y0 > y1) {
        y0 = -y0; y1 = -y1;
        yflag = 1;
    }

    if (y1 - y0 > x1 - x0) {
        temp = x0; x0 = y0; y0 = temp;
        temp = x1; x1 = y1; y1 = temp;
        xyflag = 1;
    }

    dx = x1 - x0;
    dy = y1 - y0;
    incrE = 2 * dy;
    incrNE = 2 * (dy - dx);
    d = 2 * dy - dx;
    x = x0; y = y0;

    /* 画起始点 */
    if (yflag && xyflag)      _ssd1306_draw_point(self, y, -x);
    else if (yflag)           _ssd1306_draw_point(self, x, -y);
    else if (xyflag)           _ssd1306_draw_point(self, y, x);
    else                       _ssd1306_draw_point(self, x, y);

    while (x < x1) {
        x++;
        if (d < 0) { d += incrE; }
        else       { y++; d += incrNE; }

        if (yflag && xyflag)      _ssd1306_draw_point(self, y, -x);
        else if (yflag)           _ssd1306_draw_point(self, x, -y);
        else if (xyflag)           _ssd1306_draw_point(self, y, x);
        else                       _ssd1306_draw_point(self, x, y);
    }
}

static void _ssd1306_draw_rect(OledDisplay *self, int16_t x, int16_t y,
                               uint8_t width, uint8_t height, uint8_t filled)
{
    if (!filled) {
        for (int16_t i = x; i < x + width; i++) {
            _ssd1306_draw_point(self, i, y);
            _ssd1306_draw_point(self, i, y + height - 1);
        }
        for (int16_t i = y; i < y + height; i++) {
            _ssd1306_draw_point(self, x, i);
            _ssd1306_draw_point(self, x + width - 1, i);
        }
    } else {
        for (int16_t i = x; i < x + width; i++) {
            for (int16_t j = y; j < y + height; j++) {
                _ssd1306_draw_point(self, i, j);
            }
        }
    }
}

static void _ssd1306_draw_triangle(OledDisplay *self,
                                   int16_t x0, int16_t y0,
                                   int16_t x1, int16_t y1,
                                   int16_t x2, int16_t y2, uint8_t filled)
{
    int16_t vx[] = {x0, x1, x2};
    int16_t vy[] = {y0, y1, y2};

    if (!filled) {
        _ssd1306_draw_line(self, x0, y0, x1, y1);
        _ssd1306_draw_line(self, x0, y0, x2, y2);
        _ssd1306_draw_line(self, x1, y1, x2, y2);
    } else {
        int16_t minx = x0, miny = y0, maxx = x0, maxy = y0;
        if (x1 < minx) minx = x1;
        if (x2 < minx) minx = x2;
        if (y1 < miny) miny = y1;
        if (y2 < miny) miny = y2;
        if (x1 > maxx) maxx = x1;
        if (x2 > maxx) maxx = x2;
        if (y1 > maxy) maxy = y1;
        if (y2 > maxy) maxy = y2;

        for (int16_t i = minx; i <= maxx; i++) {
            for (int16_t j = miny; j <= maxy; j++) {
                if (oled_pnpoly(3, vx, vy, i, j)) {
                    _ssd1306_draw_point(self, i, j);
                }
            }
        }
    }
}

static void _ssd1306_draw_circle(OledDisplay *self, int16_t x, int16_t y,
                                 uint8_t radius, uint8_t filled)
{
    int16_t cx = 0, cy = radius;
    int16_t d = 1 - radius;

    /* 八分之一圆弧起始点 */
    _ssd1306_draw_point(self, x + cx, y + cy);
    _ssd1306_draw_point(self, x - cx, y - cy);
    _ssd1306_draw_point(self, x + cy, y + cx);
    _ssd1306_draw_point(self, x - cy, y - cx);

    if (filled) {
        for (int16_t j = -cy; j < cy; j++) {
            _ssd1306_draw_point(self, x, y + j);
        }
    }

    while (cx < cy) {
        cx++;
        if (d < 0) { d += 2 * cx + 1; }
        else       { cy--; d += 2 * (cx - cy) + 1; }

        /* 每个八分之一圆弧 */
        _ssd1306_draw_point(self, x + cx, y + cy);
        _ssd1306_draw_point(self, x + cy, y + cx);
        _ssd1306_draw_point(self, x - cx, y - cy);
        _ssd1306_draw_point(self, x - cy, y - cx);
        _ssd1306_draw_point(self, x + cx, y - cy);
        _ssd1306_draw_point(self, x + cy, y - cx);
        _ssd1306_draw_point(self, x - cx, y + cy);
        _ssd1306_draw_point(self, x - cy, y + cx);

        if (filled) {
            for (int16_t j = -cy; j < cy; j++) {
                _ssd1306_draw_point(self, x + cx, y + j);
                _ssd1306_draw_point(self, x - cx, y + j);
            }
            for (int16_t j = -cx; j < cx; j++) {
                _ssd1306_draw_point(self, x - cy, y + j);
                _ssd1306_draw_point(self, x + cy, y + j);
            }
        }
    }
}

static void _ssd1306_draw_ellipse(OledDisplay *self, int16_t x, int16_t y,
                                  uint8_t a, uint8_t b, uint8_t filled)
{
    int16_t cx = 0, cy = b;
    int16_t ai = a, bi = b;
    float d1, d2;

    if (filled) {
        for (int16_t j = -cy; j < cy; j++) {
            _ssd1306_draw_point(self, x, y + j);
        }
    }

    _ssd1306_draw_point(self, x + cx, y + cy);
    _ssd1306_draw_point(self, x - cx, y - cy);
    _ssd1306_draw_point(self, x - cx, y + cy);
    _ssd1306_draw_point(self, x + cx, y - cy);

    d1 = bi * bi + ai * ai * (-bi + 0.5f);

    /* 椭圆中间部分 */
    while (bi * bi * (cx + 1) < ai * ai * (cy - 0.5f)) {
        if (d1 <= 0) { d1 += bi * bi * (2 * cx + 3); }
        else         { d1 += bi * bi * (2 * cx + 3) + ai * ai * (-2 * cy + 2); cy--; }
        cx++;

        if (filled) {
            for (int16_t j = -cy; j < cy; j++) {
                _ssd1306_draw_point(self, x + cx, y + j);
                _ssd1306_draw_point(self, x - cx, y + j);
            }
        }

        _ssd1306_draw_point(self, x + cx, y + cy);
        _ssd1306_draw_point(self, x - cx, y - cy);
        _ssd1306_draw_point(self, x - cx, y + cy);
        _ssd1306_draw_point(self, x + cx, y - cy);
    }

    /* 椭圆两侧部分 */
    d2 = bi * bi * (cx + 0.5f) * (cx + 0.5f)
       + ai * ai * (cy - 1) * (cy - 1) - ai * ai * bi * bi;

    while (cy > 0) {
        if (d2 <= 0) { d2 += bi * bi * (2 * cx + 2) + ai * ai * (-2 * cy + 3); cx++; }
        else         { d2 += ai * ai * (-2 * cy + 3); }
        cy--;

        if (filled) {
            for (int16_t j = -cy; j < cy; j++) {
                _ssd1306_draw_point(self, x + cx, y + j);
                _ssd1306_draw_point(self, x - cx, y + j);
            }
        }

        _ssd1306_draw_point(self, x + cx, y + cy);
        _ssd1306_draw_point(self, x - cx, y - cy);
        _ssd1306_draw_point(self, x - cx, y + cy);
        _ssd1306_draw_point(self, x + cx, y - cy);
    }
}

static void _ssd1306_draw_arc(OledDisplay *self, int16_t x, int16_t y,
                              uint8_t radius, int16_t start_angle, int16_t end_angle,
                              uint8_t filled)
{
    int16_t cx = 0, cy = radius;
    int16_t d = 1 - radius;

    if (oled_is_in_angle(cx, cy, start_angle, end_angle))
        _ssd1306_draw_point(self, x + cx, y + cy);
    if (oled_is_in_angle(-cx, -cy, start_angle, end_angle))
        _ssd1306_draw_point(self, x - cx, y - cy);
    if (oled_is_in_angle(cy, cx, start_angle, end_angle))
        _ssd1306_draw_point(self, x + cy, y + cx);
    if (oled_is_in_angle(-cy, -cx, start_angle, end_angle))
        _ssd1306_draw_point(self, x - cy, y - cx);

    if (filled) {
        for (int16_t j = -cy; j < cy; j++) {
            if (oled_is_in_angle(0, j, start_angle, end_angle))
                _ssd1306_draw_point(self, x, y + j);
        }
    }

    while (cx < cy) {
        cx++;
        if (d < 0) { d += 2 * cx + 1; }
        else       { cy--; d += 2 * (cx - cy) + 1; }

        if (oled_is_in_angle(cx, cy, start_angle, end_angle))
            _ssd1306_draw_point(self, x + cx, y + cy);
        if (oled_is_in_angle(cy, cx, start_angle, end_angle))
            _ssd1306_draw_point(self, x + cy, y + cx);
        if (oled_is_in_angle(-cx, -cy, start_angle, end_angle))
            _ssd1306_draw_point(self, x - cx, y - cy);
        if (oled_is_in_angle(-cy, -cx, start_angle, end_angle))
            _ssd1306_draw_point(self, x - cy, y - cx);
        if (oled_is_in_angle(cx, -cy, start_angle, end_angle))
            _ssd1306_draw_point(self, x + cx, y - cy);
        if (oled_is_in_angle(cy, -cx, start_angle, end_angle))
            _ssd1306_draw_point(self, x + cy, y - cx);
        if (oled_is_in_angle(-cx, cy, start_angle, end_angle))
            _ssd1306_draw_point(self, x - cx, y + cy);
        if (oled_is_in_angle(-cy, cx, start_angle, end_angle))
            _ssd1306_draw_point(self, x - cy, y + cx);

        if (filled) {
            for (int16_t j = -cy; j < cy; j++) {
                if (oled_is_in_angle(cx, j, start_angle, end_angle))
                    _ssd1306_draw_point(self, x + cx, y + j);
                if (oled_is_in_angle(-cx, j, start_angle, end_angle))
                    _ssd1306_draw_point(self, x - cx, y + j);
            }
            for (int16_t j = -cx; j < cx; j++) {
                if (oled_is_in_angle(-cy, j, start_angle, end_angle))
                    _ssd1306_draw_point(self, x - cy, y + j);
                if (oled_is_in_angle(cy, j, start_angle, end_angle))
                    _ssd1306_draw_point(self, x + cy, y + j);
            }
        }
    }
}

/* ── 文本显示 ────────────────────────────────────────── */

static void _ssd1306_show_image(OledDisplay *self, int16_t x, int16_t y,
                                uint8_t width, uint8_t height, const uint8_t *image)
{
    SSD1306 *oled = _ssd(self);
    int16_t page, shift;

    /* 清除目标区域 */
    for (int16_t j = y; j < y + height; j++) {
        for (int16_t i = x; i < x + width; i++) {
            if (i >= 0 && i <= 127 && j >= 0 && j <= 63) {
                oled->framebuf[j / 8][i] &= ~(0x01 << (j % 8));
            }
        }
    }

    for (uint8_t j = 0; j < (height - 1) / 8 + 1; j++) {
        for (uint8_t i = 0; i < width; i++) {
            if (x + i < 0 || x + i > 127) continue;

            page = y / 8;
            shift = y % 8;
            if (y < 0) { page -= 1; shift += 8; }

            if (page + j >= 0 && page + j <= 7) {
                oled->framebuf[page + j][x + i] |=
                    image[j * width + i] << (shift);
            }

            if (page + j + 1 >= 0 && page + j + 1 <= 7) {
                oled->framebuf[page + j + 1][x + i] |=
                    image[j * width + i] >> (8 - shift);
            }
        }
    }
}

static void _ssd1306_show_char(OledDisplay *self, int16_t x, int16_t y,
                               char ch, uint8_t font_size)
{
    if (font_size == OLED_FONT_8X16) {
        _ssd1306_show_image(self, x, y, 8, 16, OLED_F8x16[(uint8_t)ch - ' ']);
    } else if (font_size == OLED_FONT_6X8) {
        _ssd1306_show_image(self, x, y, 6, 8, OLED_F6x8[(uint8_t)ch - ' ']);
    }
}

static void _ssd1306_show_string(OledDisplay *self, int16_t x, int16_t y,
                                 const char *str, uint8_t font_size)
{
    uint16_t i = 0;
    int16_t x_offset = 0;
    char single_char[5];
    uint8_t char_len;
    uint16_t p_index;

    while (str[i] != '\0') {
#ifdef OLED_CHARSET_UTF8
        /* 提取 UTF-8 字符 */
        if ((str[i] & 0x80) == 0x00) {
            char_len = 1;
            single_char[0] = str[i++];
            single_char[1] = '\0';
        } else if ((str[i] & 0xE0) == 0xC0) {
            char_len = 2;
            single_char[0] = str[i++];
            if (str[i] == '\0') break;
            single_char[1] = str[i++];
            single_char[2] = '\0';
        } else if ((str[i] & 0xF0) == 0xE0) {
            char_len = 3;
            single_char[0] = str[i++];
            if (str[i] == '\0') break;
            single_char[1] = str[i++];
            if (str[i] == '\0') break;
            single_char[2] = str[i++];
            single_char[3] = '\0';
        } else if ((str[i] & 0xF8) == 0xF0) {
            char_len = 4;
            single_char[0] = str[i++];
            if (str[i] == '\0') break;
            single_char[1] = str[i++];
            if (str[i] == '\0') break;
            single_char[2] = str[i++];
            if (str[i] == '\0') break;
            single_char[3] = str[i++];
            single_char[4] = '\0';
        } else {
            i++; continue;
        }
#else
        /* GB2312 */
        if ((str[i] & 0x80) == 0x00) {
            char_len = 1;
            single_char[0] = str[i++];
            single_char[1] = '\0';
        } else {
            char_len = 2;
            single_char[0] = str[i++];
            if (str[i] == '\0') break;
            single_char[1] = str[i++];
            single_char[2] = '\0';
        }
#endif

        if (char_len == 1) {
            _ssd1306_show_char(self, x + x_offset, y, single_char[0], font_size);
            x_offset += font_size;
        } else {
            /* 搜索中文字模库 */
            for (p_index = 0;
                 strcmp(OLED_CF16x16[p_index].index, "") != 0;
                 p_index++) {
                if (strcmp(OLED_CF16x16[p_index].index, single_char) == 0) {
                    break;
                }
            }

            if (font_size == OLED_FONT_8X16) {
                _ssd1306_show_image(self, x + x_offset, y, 16, 16,
                                    OLED_CF16x16[p_index].data);
                x_offset += 16;
            } else if (font_size == OLED_FONT_6X8) {
                _ssd1306_show_char(self, x + x_offset, y, '?', OLED_FONT_6X8);
                x_offset += OLED_FONT_6X8;
            }
        }
    }
}

static void _ssd1306_show_num(OledDisplay *self, int16_t x, int16_t y,
                              uint32_t num, uint8_t length, uint8_t font_size)
{
    for (uint8_t i = 0; i < length; i++) {
        char digit = (char)(num / oled_pow(10, length - i - 1) % 10 + '0');
        _ssd1306_show_char(self, x + i * font_size, y, digit, font_size);
    }
}

static void _ssd1306_show_signed(OledDisplay *self, int16_t x, int16_t y,
                                 int32_t num, uint8_t length, uint8_t font_size)
{
    uint32_t num1;
    if (num >= 0) {
        _ssd1306_show_char(self, x, y, '+', font_size);
        num1 = (uint32_t)num;
    } else {
        _ssd1306_show_char(self, x, y, '-', font_size);
        num1 = (uint32_t)(-num);
    }
    for (uint8_t i = 0; i < length; i++) {
        char digit = (char)(num1 / oled_pow(10, length - i - 1) % 10 + '0');
        _ssd1306_show_char(self, x + (i + 1) * font_size, y, digit, font_size);
    }
}

static void _ssd1306_show_hex(OledDisplay *self, int16_t x, int16_t y,
                              uint32_t num, uint8_t length, uint8_t font_size)
{
    for (uint8_t i = 0; i < length; i++) {
        uint8_t single = num / oled_pow(16, length - i - 1) % 16;
        char digit = (single < 10) ? (char)(single + '0')
                                   : (char)(single - 10 + 'A');
        _ssd1306_show_char(self, x + i * font_size, y, digit, font_size);
    }
}

static void _ssd1306_show_bin(OledDisplay *self, int16_t x, int16_t y,
                              uint32_t num, uint8_t length, uint8_t font_size)
{
    for (uint8_t i = 0; i < length; i++) {
        char digit = (char)(num / oled_pow(2, length - i - 1) % 2 + '0');
        _ssd1306_show_char(self, x + i * font_size, y, digit, font_size);
    }
}

static void _ssd1306_show_float(OledDisplay *self, int16_t x, int16_t y,
                                double num, uint8_t int_len, uint8_t frac_len,
                                uint8_t font_size)
{
    uint32_t pow_num, int_num, frac_num;

    if (num >= 0) {
        _ssd1306_show_char(self, x, y, '+', font_size);
    } else {
        _ssd1306_show_char(self, x, y, '-', font_size);
        num = -num;
    }

    int_num = (uint32_t)num;
    num -= int_num;
    pow_num = oled_pow(10, frac_len);
    frac_num = (uint32_t)(num * pow_num + 0.5); /* 四舍五入 */
    int_num += frac_num / pow_num;

    _ssd1306_show_num(self, x + font_size, y, int_num, int_len, font_size);
    _ssd1306_show_char(self, x + (int_len + 1) * font_size, y, '.', font_size);
    _ssd1306_show_num(self, x + (int_len + 2) * font_size, y, frac_num, frac_len, font_size);
}

/* ── 格式化输出 ──────────────────────────────────────── */

void oled_printf(OledDisplay *self, int16_t x, int16_t y,
                 uint8_t font_size, const char *fmt, ...)
{
    char str[256];
    va_list arg;
    va_start(arg, fmt);
    vsnprintf(str, sizeof(str), fmt, arg);
    va_end(arg);
    _ssd1306_show_string(self, x, y, str, font_size);
}

/* ═══════════════════════════════════════════════════════
 * vtable 定义
 * ═══════════════════════════════════════════════════════ */

static const OledVtable _ssd1306_vtable = {
    /* 硬件控制 */
    .init         = _ssd1306_init,
    .clear        = _ssd1306_clear,
    .flush        = _ssd1306_flush,
    .flush_area   = _ssd1306_flush_area,
    .reverse      = _ssd1306_reverse,
    .reverse_area = _ssd1306_reverse_area,

    /* 像素操作 */
    .draw_point   = _ssd1306_draw_point,
    .get_point    = _ssd1306_get_point,

    /* 几何绘图 */
    .draw_line    = _ssd1306_draw_line,
    .draw_rect    = _ssd1306_draw_rect,
    .draw_triangle = _ssd1306_draw_triangle,
    .draw_circle  = _ssd1306_draw_circle,
    .draw_ellipse = _ssd1306_draw_ellipse,
    .draw_arc     = _ssd1306_draw_arc,

    /* 文本显示 */
    .show_char    = _ssd1306_show_char,
    .show_string  = _ssd1306_show_string,
    .show_num     = _ssd1306_show_num,
    .show_signed  = _ssd1306_show_signed,
    .show_hex     = _ssd1306_show_hex,
    .show_bin     = _ssd1306_show_bin,
    .show_float   = _ssd1306_show_float,
    .show_image   = _ssd1306_show_image,

    /* 格式化输出 */
    /* NOTE: varargs 不能通过函数指针安全调用 (C 标准未定义行为)。
       oled_printf 是独立函数，直接调用 ssd1306 内部实现。
       此槽位故意为 NULL — 应使用 oled_printf() 而非 vtable->printf() */
    .printf       = NULL,
};

/* ═══════════════════════════════════════════════════════
 * 构造函数
 * ═══════════════════════════════════════════════════════ */

void ssd1306_ctor(SSD1306 *self, void *i2c, uint8_t addr)
{
    self->base.vtable = &_ssd1306_vtable;
    self->i2c  = i2c;
    self->addr = addr;
    memset(self->framebuf, 0, sizeof(self->framebuf));
}
