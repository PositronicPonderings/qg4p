/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    m2_demo.c
 * @brief   Milestone 2 test: palette functions and the QuickBasic primitives,
 *          as a gallery of pages shown on both screens.
 *
 * Built by tests/hardware/CMakeLists.txt as its own target (qg4p_m2.uf2).
 *
 * PAGES (about 4 seconds each; the serial monitor says what to look for and
 * how long each page took to draw on each screen)
 *
 *   1  Lines       Starburst of thin lines, then widths 1..8
 *   2  Boxes       Outline / filled / both, at three line widths
 *   3  Circles     Outline, filled, both, thick rings, concentric rings
 *   4  Arcs        An animated "dial" gauge, plus quarter-circle arcs
 *   5  Palette     qg_color_from_rgb() swatches and qg_palette_set()
 *   6  Clipping    Shapes hanging off every edge
 *
 * All sizes are worked out from each screen's width and height, so the same
 * code fills the 240x320 and 320x480 screens sensibly.
 */
#include <stdio.h>
#include <math.h>
#include "pico/stdlib.h"
#include "qg4p.h"
#include "demo_setup.h"

#define PAGE_MS 4000

/* Time how long drawing a page takes on one screen, and report it. */
typedef void (*page_fn)(qg_screen_t *s);

static void run_page(const char *title, const char *look_for, page_fn draw)
{
    printf("\n--- %s ---\n  Look for: %s\n", title, look_for);

    qg_screen_t *screens[2] = { &scr_a, &scr_b };
    const char   *names[2]   = { "A", "B" };

    for (int i = 0; i < 2; i++) {
        qg_screen_t *s = screens[i];
        qg_screen_set_line_width(s, 1);
        qg_cls(s, QG_BLACK);

        uint64_t t0 = time_us_64();
        draw(s);
        uint64_t us = time_us_64() - t0;
        printf("  %-6s drawn in %lu.%lu ms\n", names[i],
               (unsigned long)(us / 1000), (unsigned long)((us % 1000) / 100));
    }
    sleep_ms(PAGE_MS);
}

/* ========================================================================== */
/*  Page 1: lines                                                             */
/* ========================================================================== */
static void page_lines(qg_screen_t *s)
{
    const int16_t w = qg_screen_width(s), h = qg_screen_height(s);

    /* Starburst in the top half: 24 thin lines from the centre, cycling
     * through the bright named colours (9..15).                             */
    const int16_t cx = w / 2, cy = h / 4;
    const float   r  = (float)(h / 4 - 4);
    for (int i = 0; i < 24; i++) {
        float a = (float)i * (2.0f * 3.14159265f / 24.0f);
        qg_line(s, cx, cy,
                 (int16_t)(cx + r * cosf(a)), (int16_t)(cy - r * sinf(a)),
                 (qg_color_t)(QG_LIGHTBLUE + i % 7));
    }

    /* Bottom half: widths 1..8, each as a diagonal and a horizontal line.
     * The horizontal lines are spaced by their own width plus a 2 px gap.   */
    int16_t hy = (int16_t)(h - 58);
    for (int t = 1; t <= 8; t++) {
        qg_screen_set_line_width(s, (uint8_t)t);
        int16_t x = (int16_t)(8 + (t - 1) * (w - 16) / 8);
        qg_line(s, x, (int16_t)(h / 2 + 10), (int16_t)(x + w / 12), (int16_t)(h - 70),
                 QG_YELLOW);
        qg_line(s, 10, hy, (int16_t)(w - 10), hy, QG_LIGHTCYAN);
        hy = (int16_t)(hy + t + 2);
    }
}

/* ========================================================================== */
/*  Page 2: boxes                                                             */
/* ========================================================================== */
static void page_boxes(qg_screen_t *s)
{
    const int16_t w = qg_screen_width(s), h = qg_screen_height(s);
    const int16_t cw = w / 3, rh = h / 3;           /* 3 x 3 grid of cells */
    const uint8_t widths[3] = { 1, 3, 6 };

    for (int row = 0; row < 3; row++) {
        qg_screen_set_line_width(s, widths[row]);
        for (int col = 0; col < 3; col++) {
            int16_t x1 = (int16_t)(col * cw + 6), y1 = (int16_t)(row * rh + 6);
            int16_t x2 = (int16_t)(x1 + cw - 12),  y2 = (int16_t)(y1 + rh - 12);
            switch (col) {
            case 0: qg_box(s, x1, y1, x2, y2, QG_WHITE, QG_TRANSPARENT); break; /* B  */
            case 1: qg_box(s, x1, y1, x2, y2, QG_TRANSPARENT, QG_BLUE);  break; /* BF */
            case 2: qg_box(s, x1, y1, x2, y2, QG_YELLOW, QG_RED);        break; /* both */
            }
        }
    }
}

/* ========================================================================== */
/*  Page 3: circles and ellipses                                              */
/* ========================================================================== */
static void page_circles(qg_screen_t *s)
{
    const int16_t w = qg_screen_width(s), h = qg_screen_height(s);
    const int16_t r = (int16_t)(w / 7);

    /* Top row: outline, filled, filled + outline. */
    qg_circle(s, w / 6,     r + 6, r, QG_WHITE, QG_TRANSPARENT);
    qg_circle(s, w / 2,     r + 6, r, QG_TRANSPARENT, QG_GREEN);
    qg_circle(s, 5 * w / 6, r + 6, r, QG_YELLOW, QG_MAGENTA);

    /* Middle: an ellipse with a 4-pixel outline, and a tall one. */
    qg_screen_set_line_width(s, 4);
    qg_ellipse(s, w / 3, h / 2, (int16_t)(w / 4), (int16_t)(h / 10),
                QG_LIGHTCYAN, QG_BLUE);
    qg_ellipse(s, (int16_t)(5 * w / 6), h / 2, (int16_t)(w / 10), (int16_t)(h / 7),
                QG_LIGHTRED, QG_TRANSPARENT);

    /* Bottom: concentric rings of width 3 in the colour cube's reds/oranges. */
    qg_screen_set_line_width(s, 3);
    const int16_t by = (int16_t)(h - h / 5);
    for (int i = 0; i < 6; i++) {
        qg_circle(s, w / 2, by, (int16_t)(h / 6 - i * 5),
                   (qg_color_t)(16 + 36 * 5 + 6 * i), QG_TRANSPARENT);
    }
}

/* ========================================================================== */
/*  Page 4: arcs, including a small animation                                 */
/* ========================================================================== */
static void page_arcs(qg_screen_t *s)
{
    const int16_t w = qg_screen_width(s), h = qg_screen_height(s);
    const int16_t r  = (int16_t)(w / 2 - 12);
    const int16_t cx = w / 2, cy = (int16_t)(r + 8);    /* whole dial on screen */

    /* A dial gauge: a 270-degree track, open at the bottom. It runs from
     * lower-left (225 degrees) over the top to lower-right (-45 degrees).
     * Arcs are drawn COUNTER-clockwise from start to end, so to get that
     * shape we start at the lower-RIGHT: start = -45, end = 225.            */
    qg_screen_set_line_width(s, 10);
    qg_arc(s, cx, cy, r, r, -45, 225, QG_DARKGRAY);

    /* Fill the gauge clockwise, a slice at a time, drawing ONLY the new slice
     * each step. On a DIRECT screen, redrawing just what changed is what
     * keeps animation smooth.                                               */
    for (int deg = 225; deg > -45; deg -= 10) {
        qg_color_t c = (deg > 45) ? QG_LIGHTGREEN : (deg > 0 ? QG_YELLOW : QG_LIGHTRED);
        qg_arc(s, cx, cy, r, r, (int16_t)(deg - 10), (int16_t)deg, c);
        sleep_ms(15);
    }

    /* Below: four 1-pixel quarter arcs forming a circle, one colour each,
     * and a thick 3/4 arc next to them.                                     */
    qg_screen_set_line_width(s, 1);
    const int16_t qy = (int16_t)(h - h / 5), qr = (int16_t)(w / 6);
    qg_arc(s, w / 4, qy, qr, qr,   0,  90, QG_RED);
    qg_arc(s, w / 4, qy, qr, qr,  90, 180, QG_GREEN);
    qg_arc(s, w / 4, qy, qr, qr, 180, 270, QG_BLUE);
    qg_arc(s, w / 4, qy, qr, qr, 270, 360, QG_YELLOW);

    qg_screen_set_line_width(s, 5);
    qg_arc(s, (int16_t)(3 * w / 4), qy, qr, qr, 90, 0, QG_LIGHTMAGENTA);
}

/* ========================================================================== */
/*  Page 5: palette                                                           */
/* ========================================================================== */
static void page_palette(qg_screen_t *s)
{
    const int16_t w = qg_screen_width(s), h = qg_screen_height(s);

    /* Row of swatches picked by RGB. qg_color_from_rgb() finds the nearest
     * entry once; the result is an ordinary colour after that.              */
    static const uint8_t rgb[6][3] = {
        { 212, 175,  55 },   /* gold      */
        { 255, 128,   0 },   /* orange    */
        {   0, 128, 128 },   /* teal      */
        { 255, 105, 180 },   /* pink      */
        { 128,   0, 128 },   /* purple    */
        { 139,  69,  19 },   /* saddle brown */
    };
    const int16_t sw = (int16_t)(w / 6);
    for (int i = 0; i < 6; i++) {
        qg_color_t c = qg_color_from_rgb(rgb[i][0], rgb[i][1], rgb[i][2], s->palette);
        if (s == &scr_a) {
            printf("  RGB(%3u,%3u,%3u) -> index %u\n",
                   rgb[i][0], rgb[i][1], rgb[i][2], (unsigned)c);
        }
        qg_box(s, (int16_t)(i * sw + 2), 4, (int16_t)((i + 1) * sw - 3), (int16_t)(h / 5),
                QG_TRANSPARENT, c);
    }

    /* Grey ramp: the 23 greys at indices 232..254. */
    const int16_t gy = (int16_t)(h / 5 + 10);
    for (int i = 0; i < 23; i++) {
        int16_t x1 = (int16_t)(i * w / 23), x2 = (int16_t)((i + 1) * w / 23 - 1);
        qg_box(s, x1, gy, x2, (int16_t)(gy + h / 10), QG_TRANSPARENT, (qg_color_t)(232 + i));
    }

    /* qg_palette_set() on a DIRECT screen: bar A is drawn with index 240
     * (a mid grey). Then index 240 is changed to gold and bar B is drawn with
     * the same index. Bar A stays grey: pixels already on the glass keep the
     * colour they were drawn with. (On a BUF8 screen, both would turn gold.) */
    const int16_t by = (int16_t)(h / 2 + 10), bh = (int16_t)(h / 8);
    qg_box(s, 10, by, (int16_t)(w / 2 - 6), (int16_t)(by + bh), QG_WHITE, 240);   /* A */
    qg_palette_set(s, 240, 212, 175, 55);
    qg_box(s, (int16_t)(w / 2 + 5), by, (int16_t)(w - 11), (int16_t)(by + bh), QG_WHITE, 240); /* B */
    qg_palette_reset(s);

    /* The six colour-cube reds, darkest to brightest, as a final strip. */
    const int16_t cy = (int16_t)(h - h / 6);
    for (int i = 0; i < 6; i++) {
        qg_box(s, (int16_t)(i * sw + 2), cy, (int16_t)((i + 1) * sw - 3), (int16_t)(h - 6),
                QG_TRANSPARENT, (qg_color_t)(16 + 36 * i));
    }
}

/* ========================================================================== */
/*  Page 6: clipping                                                          */
/* ========================================================================== */
static void page_clipping(qg_screen_t *s)
{
    const int16_t w = qg_screen_width(s), h = qg_screen_height(s);

    qg_screen_set_line_width(s, 3);
    qg_circle(s, 0, 0, (int16_t)(w / 4), QG_WHITE, QG_BLUE);            /* top-left     */
    qg_circle(s, w, h, (int16_t)(w / 4), QG_WHITE, QG_RED);             /* bottom-right */
    qg_ellipse(s, w, 0, (int16_t)(w / 3), (int16_t)(h / 8), QG_YELLOW, QG_GREEN);
    qg_box(s, -30, (int16_t)(h - 60), 40, (int16_t)(h + 30), QG_LIGHTCYAN, QG_MAGENTA);

    qg_screen_set_line_width(s, 1);
    qg_line(s, -1000, (int16_t)(h / 2), 1000, (int16_t)(h / 2 + 20), QG_WHITE);
    qg_screen_set_line_width(s, 6);
    qg_line(s, (int16_t)(w / 2), -500, (int16_t)(w / 2 + 30), 800, QG_LIGHTGREEN);
}

/* ========================================================================== */
int main(void)
{
    demo_setup("Dice Roller qg4p - Milestone 2");

    while (true) {
        run_page("Page 1: lines",
                 "starburst with no gaps at the centre; widths 1..8 growing evenly",
                 page_lines);
        run_page("Page 2: boxes",
                 "columns: outline / filled / both. Rows: widths 1, 3, 6, drawn inward",
                 page_boxes);
        run_page("Page 3: circles",
                 "round shapes; outlines hug the fills with no gaps; even thick rings",
                 page_circles);
        run_page("Page 4: arcs",
                 "gauge fills clockwise green -> yellow -> red; four quarters make a circle",
                 page_arcs);
        run_page("Page 5: palette",
                 "6 RGB swatches, smooth grey ramp; bar A GREY and bar B GOLD",
                 page_palette);
        run_page("Page 6: clipping",
                 "shapes cut off cleanly at every edge, nothing wrapping around",
                 page_clipping);
    }
}
