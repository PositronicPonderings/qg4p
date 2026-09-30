/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    colour_check.c
 * @brief   Example 15: a colour test pattern, for settling "is it the panel,
 *          my settings, or my camera?" arguments.
 *
 * Target: qg4p_colour_check.   Screens: A (or A and B: see below).
 * Expected: examples/expected/colour_check.png and colour_check_diagnostics.png
 *
 * Two pages, alternating every 8 seconds:
 *   SWATCHES     the 16 named colours, each with its name and its exact RGB
 *                values (as stored; a colour adjustment from board.h changes
 *                what's actually sent, which is the point of it)
 *   DIAGNOSTICS  boxes labelled with what they SHOULD be (red, green, blue,
 *                white, black), smooth ramps, and a label in each corner
 *
 * What wrong looks like, and the fix, is in examples/README.md under
 * "Help, my red looks blue". The short version:
 *   the RED box looks blue       -> flip bgr      (in board.h)
 *   WHITE looks black            -> flip invert
 *   corner labels read backwards -> flip mirror_x
 *   right colours, but faded or shifting as you tilt: that's the panel's
 *   viewing angle, not a setting
 */
#include <stdio.h>
#include "pico/stdlib.h"
#include "board.h"

/* 0: screen A only. 1: both screens at once, for comparing two panels. */
#define BOTH_SCREENS  0

/* ========================================================================== */
/*  Page 1: the named colours                                                 */
/* ========================================================================== */
static void swatches(qg_screen_t *s)
{
    const int16_t w = qg_screen_width(s), h = qg_screen_height(s);
    const int16_t top = 28, cols = 2, rows = 8;
    const int16_t cw = (int16_t)(w / cols), ch = (int16_t)((h - top) / rows);
    char buf[24];

    qg_cls(s, QG_BLACK);
    qg_print_at(s, 4, 2, "{f:1}Named colours", QG_WHITE, NULL);
    qg_screen_set_wrap(s, false);           /* labels stay on one line */

    for (int i = 0; i < 16; i++) {
        int16_t x = (int16_t)((i / rows) * cw), y = (int16_t)(top + (i % rows) * ch);
        int16_t sw = (int16_t)(ch - 8);                        /* a square swatch */

        /* The swatch, outlined in grey so black and dark blue still show. */
        qg_box(s, (int16_t)(x + 2), (int16_t)(y + 2), (int16_t)(x + 1 + sw), (int16_t)(y + 1 + sw),
               QG_DARKGRAY, (qg_color_t)i);

        /* Its name, and its RGB as stored in the palette. */
        uint8_t r, g, b;
        qg_palette_get(s->palette, (qg_color_t)i, &r, &g, &b);
        qg_print_at(s, (int16_t)(x + sw + 6), (int16_t)(y + 2), qg_color_name((qg_color_t)i),
                    QG_WHITE, &font_small);
        snprintf(buf, sizeof buf, "%3u,%3u,%3u", r, g, b);
        qg_print_at(s, (int16_t)(x + sw + 6), (int16_t)(y + 2 + ch / 2), buf, QG_LIGHTGRAY, &font_small);
    }
    qg_screen_set_wrap(s, true);
}

/* ========================================================================== */
/*  Page 2: diagnostics                                                       */
/* ========================================================================== */
#define RAMP_FIRST 160          /* palette entries 160..223: four ramps of 16 */
#define STEPS      16

static void diagnostics(qg_screen_t *s)
{
    const int16_t w = qg_screen_width(s), h = qg_screen_height(s);

    /* Pure colours, set explicitly so they're exactly 255 (the named RED is
     * QuickBasic's softer 170). Entries 250..254 are borrowed for this.    */
    qg_palette_set(s, 250, 255, 0, 0);
    qg_palette_set(s, 251, 0, 255, 0);
    qg_palette_set(s, 252, 0, 0, 255);
    qg_palette_set(s, 253, 255, 255, 255);
    qg_palette_set(s, 254, 0, 0, 0);

    /* Ramps: 16 steps from black to full, in entries 160..223. */
    for (int k = 0; k < STEPS; k++) {
        uint8_t v = (uint8_t)(k * 255 / (STEPS - 1));
        qg_palette_set(s, (qg_color_t)(RAMP_FIRST + k),             v, 0, 0);
        qg_palette_set(s, (qg_color_t)(RAMP_FIRST + STEPS + k),     0, v, 0);
        qg_palette_set(s, (qg_color_t)(RAMP_FIRST + 2 * STEPS + k), 0, 0, v);
        qg_palette_set(s, (qg_color_t)(RAMP_FIRST + 3 * STEPS + k), v, v, v);
    }

    qg_cls(s, QG_DARKGRAY);

    /* Five labelled boxes across the top half. Labels are black or white,
     * whichever reads best on the box's INTENDED colour.                   */
    static const struct { qg_color_t c; const char *name; qg_color_t ink; } box[5] = {
        { 250, "{f:2}RED",   QG_WHITE }, { 251, "{f:2}GREEN", QG_BLACK }, { 252, "{f:2}BLUE",  QG_WHITE },
        { 253, "{f:2}WHITE", QG_BLACK }, { 254, "{f:2}BLACK", QG_WHITE },
    };
    const int16_t bt = (int16_t)(h * 10 / 100), bh = (int16_t)(h * 34 / 100);
    const int16_t bw = (int16_t)(w / 5);
    for (int i = 0; i < 5; i++) {
        int16_t x = (int16_t)(i * bw);
        qg_box(s, x, bt, (int16_t)(x + bw - 1), (int16_t)(bt + bh), QG_TRANSPARENT, box[i].c);
        qg_screen_set_colors(s, box[i].ink, QG_DEFAULT);   /* qg_print_box uses the default colour */
        qg_print_box(s, x, (int16_t)(bt + bh / 2 - 7), bw, box[i].name, QG_ALIGN_CENTER);
    }
    qg_screen_set_colors(s, QG_WHITE, QG_DEFAULT);

    /* Ramps: each should climb evenly from black to full, with no step
     * suddenly brighter, darker or a different hue.                        */
    static const char *ramp_name[4] = { "R", "G", "B", "grey" };
    const int16_t rt = (int16_t)(bt + bh + h * 4 / 100), rh = (int16_t)(h * 9 / 100);
    const int16_t lx = 34, sw = (int16_t)((w - lx - 4) / STEPS);
    /* On black: against the grey page, the step nearest that grey would
     * seem to vanish and look like a gap.                                  */
    qg_box(s, 0, (int16_t)(rt - 3), (int16_t)(w - 1), (int16_t)(rt + 4 * (rh + 3)), QG_TRANSPARENT, QG_BLACK);
    for (int r = 0; r < 4; r++) {
        int16_t y = (int16_t)(rt + r * (rh + 3));
        qg_print_at(s, 4, (int16_t)(y + rh / 2 - 7), ramp_name[r], QG_WHITE, &font_small);
        for (int k = 0; k < STEPS; k++) {
            int16_t x = (int16_t)(lx + k * sw);
            qg_box(s, x, y, (int16_t)(x + sw - 1), (int16_t)(y + rh - 1), QG_TRANSPARENT,
                   (qg_color_t)(RAMP_FIRST + r * STEPS + k));
        }
    }

    /* Corner labels: all four should read normally, each in its own
     * corner. Backwards text means mirroring; swapped corners, rotation.   */
    qg_print_at(s, 2, 2, "top left", QG_YELLOW, &font_small);
    int16_t tw;
    qg_text_measure(s, "top right", &font_small, 0, &tw, NULL);
    qg_print_at(s, (int16_t)(w - tw - 2), 2, "top right", QG_YELLOW, &font_small);
    qg_print_at(s, 2, (int16_t)(h - 16), "bottom left", QG_YELLOW, &font_small);
    qg_text_measure(s, "bottom right", &font_small, 0, &tw, NULL);
    qg_print_at(s, (int16_t)(w - tw - 2), (int16_t)(h - 16), "bottom right", QG_YELLOW, &font_small);
}

/* ========================================================================== */
int main(void)
{
#if BOTH_SCREENS
    board_init_two();
#else
    board_init();
#endif
    while (true) {
        swatches(&screen_a);
#if BOTH_SCREENS
        swatches(&screen_b);
#endif
        sleep_ms(8000);

        diagnostics(&screen_a);
#if BOTH_SCREENS
        diagnostics(&screen_b);
#endif
        sleep_ms(8000);
        qg_palette_reset(&screen_a);       /* put the borrowed entries back */
#if BOTH_SCREENS
        qg_palette_reset(&screen_b);
#endif
    }
}
