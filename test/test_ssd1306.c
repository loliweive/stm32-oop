/**
 * @file    test_ssd1306.c
 * @brief   SSD1306 OLED 驱动单元测试 — vtable 派发 + 帧缓冲操作
 */

#include "unity.h"
#include "ssd1306.h"
#include "oled.h"
#include <string.h>

static SSD1306 oled;

static void setup(void)
{
    memset(&oled, 0, sizeof(oled));
    ssd1306_ctor(&oled, NULL, 0x3C);
}

/* ── 构造函数 ──────────────────────────────────────── */

TEST(ctor_sets_vtable)
{
    setup();
    TEST_ASSERT_NOT_NULL(oled.base.vtable);
}

TEST(ctor_clears_framebuf)
{
    uint8_t p, c;
    setup();
    for (p = 0; p < SSD1306_PAGES; p++)
        for (c = 0; c < SSD1306_WIDTH; c++)
            TEST_ASSERT_EQUAL_U8(0x00, oled.framebuf[p][c]);
}

TEST(ctor_sets_i2c_addr)
{
    setup();
    TEST_ASSERT_NULL(oled.i2c);
    TEST_ASSERT_EQUAL_U8(0x3C, oled.addr);
}

/* ── draw_point ─────────────────────────────────────── */

TEST(point_top_left)
{
    OledDisplay *d = &oled.base;
    setup();
    oled_draw_point(d, 0, 0);
    TEST_ASSERT_TRUE(oled.framebuf[0][0] & 0x01);
}

TEST(point_bottom_right)
{
    OledDisplay *d = &oled.base;
    setup();
    oled_draw_point(d, 127, 63);
    TEST_ASSERT_TRUE(oled.framebuf[7][127] & 0x80);
}

TEST(point_middle)
{
    OledDisplay *d = &oled.base;
    setup();
    oled_draw_point(d, 64, 13);
    TEST_ASSERT_TRUE(oled.framebuf[1][64] & (1 << 5));
}

TEST(get_point_returns_correct)
{
    OledDisplay *d = &oled.base;
    setup();
    oled_draw_point(d, 10, 20);
    TEST_ASSERT_EQUAL_U8(1, oled_get_point(d, 10, 20));
    TEST_ASSERT_EQUAL_U8(0, oled_get_point(d, 11, 20));
}

TEST(point_clips_out_of_bounds)
{
    OledDisplay *d = &oled.base;
    uint8_t p, c;
    setup();
    oled_draw_point(d, -1, 0);
    oled_draw_point(d, 0, -1);
    oled_draw_point(d, 128, 0);
    oled_draw_point(d, 0, 64);
    for (p = 0; p < 8; p++)
        for (c = 0; c < 128; c++)
            TEST_ASSERT_EQUAL_U8(0x00, oled.framebuf[p][c]);
}

/* ── clear ─────────────────────────────────────────── */

TEST(clear_zeros_framebuf)
{
    OledDisplay *d = &oled.base;
    uint8_t p, c;
    setup();
    for (p = 0; p < 8; p++) memset(oled.framebuf[p], 0xFF, 128);
    oled_clear(d);
    for (p = 0; p < 8; p++)
        for (c = 0; c < 128; c++)
            TEST_ASSERT_EQUAL_U8(0x00, oled.framebuf[p][c]);
}

/* ── reverse ───────────────────────────────────────── */

TEST(reverse_flips_bits)
{
    OledDisplay *d = &oled.base;
    setup();
    oled_draw_point(d, 0, 0);
    oled_draw_point(d, 63, 31);
    oled_reverse(d);
    TEST_ASSERT_EQUAL_U8(0, oled_get_point(d, 0, 0));
    TEST_ASSERT_EQUAL_U8(1, oled_get_point(d, 1, 0));
}

/* ── line ──────────────────────────────────────────── */

TEST(line_horizontal)
{
    OledDisplay *d = &oled.base;
    setup();
    oled_draw_line(d, 0, 0, 10, 0);
    TEST_ASSERT_EQUAL_U8(1, oled_get_point(d, 0, 0));
    TEST_ASSERT_EQUAL_U8(1, oled_get_point(d, 10, 0));
    TEST_ASSERT_EQUAL_U8(0, oled_get_point(d, 11, 0));
}

TEST(line_vertical)
{
    OledDisplay *d = &oled.base;
    setup();
    oled_draw_line(d, 5, 0, 5, 10);
    TEST_ASSERT_EQUAL_U8(1, oled_get_point(d, 5, 0));
    TEST_ASSERT_EQUAL_U8(1, oled_get_point(d, 5, 10));
}

/* ── rect ──────────────────────────────────────────── */

TEST(rect_unfilled_border)
{
    OledDisplay *d = &oled.base;
    setup();
    oled_draw_rect(d, 0, 0, 10, 10, OLED_UNFILLED);
    TEST_ASSERT_EQUAL_U8(1, oled_get_point(d, 0, 0));
    TEST_ASSERT_EQUAL_U8(0, oled_get_point(d, 5, 5));
}

TEST(rect_filled_pixels)
{
    OledDisplay *d = &oled.base;
    setup();
    oled_draw_rect(d, 2, 3, 4, 5, OLED_FILLED);
    TEST_ASSERT_EQUAL_U8(1, oled_get_point(d, 2, 3));
    TEST_ASSERT_EQUAL_U8(1, oled_get_point(d, 5, 7));
}

/* ── show_char / show_string ──────────────────────── */

TEST(show_char_has_data)
{
    OledDisplay *d = &oled.base;
    uint8_t p, c, total = 0;
    setup();
    oled_show_char(d, 0, 0, 'A', OLED_FONT_8X16);
    for (p = 0; p < 2; p++)
        for (c = 0; c < 8; c++)
            total |= oled.framebuf[p][c];
    TEST_ASSERT_TRUE(total != 0);
}

TEST(show_string_has_data)
{
    OledDisplay *d = &oled.base;
    uint8_t p, c, total = 0;
    setup();
    oled_show_string(d, 0, 0, "AB", OLED_FONT_8X16);
    for (p = 0; p < 2; p++)
        for (c = 0; c < 16; c++)
            total |= oled.framebuf[p][c];
    TEST_ASSERT_TRUE(total != 0);
}

/* ── circle ────────────────────────────────────────── */

TEST(circle_circumference)
{
    OledDisplay *d = &oled.base;
    setup();
    oled_draw_circle(d, 20, 20, 5, OLED_UNFILLED);
    TEST_ASSERT_EQUAL_U8(1, oled_get_point(d, 25, 20));
    TEST_ASSERT_EQUAL_U8(0, oled_get_point(d, 30, 20));
}

TEST(filled_circle_center)
{
    OledDisplay *d = &oled.base;
    setup();
    oled_draw_circle(d, 20, 20, 5, OLED_FILLED);
    TEST_ASSERT_EQUAL_U8(1, oled_get_point(d, 20, 20));
}

/* ── 多操作复合 ────────────────────────────────────── */

TEST(multiple_ops_accumulate)
{
    OledDisplay *d = &oled.base;
    uint8_t y;
    setup();
    oled_draw_point(d, 5, 10);
    oled_draw_point(d, 5, 11);
    oled_draw_line(d, 5, 12, 5, 15);
    for (y = 10; y <= 15; y++)
        TEST_ASSERT_EQUAL_U8(1, oled_get_point(d, 5, y));
    TEST_ASSERT_EQUAL_U8(0, oled_get_point(d, 6, 10));
}

/* ═══════════════════════════════════════════════════════
 * 测试入口
 * ═══════════════════════════════════════════════════════ */

int main(void)
{
    unity_init();

    RUN_TEST(ctor_sets_vtable);
    RUN_TEST(ctor_clears_framebuf);
    RUN_TEST(ctor_sets_i2c_addr);
    RUN_TEST(point_top_left);
    RUN_TEST(point_bottom_right);
    RUN_TEST(point_middle);
    RUN_TEST(get_point_returns_correct);
    RUN_TEST(point_clips_out_of_bounds);
    RUN_TEST(clear_zeros_framebuf);
    RUN_TEST(reverse_flips_bits);
    RUN_TEST(line_horizontal);
    RUN_TEST(line_vertical);
    RUN_TEST(rect_unfilled_border);
    RUN_TEST(rect_filled_pixels);
    RUN_TEST(show_char_has_data);
    RUN_TEST(show_string_has_data);
    RUN_TEST(circle_circumference);
    RUN_TEST(filled_circle_center);
    RUN_TEST(multiple_ops_accumulate);

    unity_summary();
    return Unity.failed ? 1 : 0;
}
