/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    dice_roller.c
 * @brief   Example 14: a two-screen dice roller. Two dice tumble across a felt
 *          table on screen B while screen A keeps a running log.
 *
 * Target: qg4p_dice_roller.   Screens: A and B (B as BUF8).
 * Expected: examples/expected/dice_roller_a.png and _b.png
 *
 * The project QG4P was born from, reduced to its happiest essentials:
 *   screen B  the table: dice bounce in, spin, and land; a histogram of
 *             every total so far sits underneath
 *   screen A  the log: every roll, in tab-aligned columns, with the
 *             noteworthy ones called out
 *
 * It rolls by itself every few seconds. To roll on demand, wire a push
 * button between a GPIO and GND and set ROLL_BUTTON_PIN below.
 *
 * Techniques on show: shapes drawn as dice (no images at all), a
 * framebuffer for a flicker-free tumble, VIEW used to repair only the
 * patches the dice passed through, percentages so it fits either screen B
 * board, markup and tabs for the log, and DIRECT-screen scrolling.
 */
#include <math.h>
#include <stdio.h>
#include "pico/stdlib.h"
#include "board.h"

#define ROLL_BUTTON_PIN  -1          /* e.g. 14; -1 = roll automatically */
#define AUTO_ROLL_MS     3500

static uint8_t fb_b[BOARD_B_WIDTH * BOARD_B_HEIGHT];

static qg_color_t felt, felt_dark, wood;      /* colours picked by RGB at start */
static int16_t    table_bottom;               /* where the table ends on B      */
static uint32_t   totals[13];                 /* how often each total 2..12 came up */
static uint32_t   roll_count;

/* ========================================================================== */
/*  Drawing a die with nothing but shapes                                     */
/* ========================================================================== */

/*
 * A die is a rounded square: two overlapping boxes (one wide, one tall) and
 * a circle in each corner fill it in. To look like it's SPINNING, we squash
 * it horizontally: squash = 1 is face-on, 0 is edge-on. Pips are ellipses
 * squashed the same way, and vanish when the die is nearly edge-on.
 */
static void draw_die(qg_screen_t *s, int16_t cx, int16_t cy, int16_t size, float squash, int value)
{
    int16_t hh = (int16_t)(size / 2);
    int16_t hw = (int16_t)(hh * squash);
    if (hw < 3) hw = 3;
    int16_t r = (int16_t)(size / 8);
    if (r > hw) r = hw;

    qg_color_t body = QG_WHITE, pip = (value == 1) ? QG_RED : QG_BLACK;
    qg_box(s, (int16_t)(cx - hw + r), (int16_t)(cy - hh), (int16_t)(cx + hw - r), (int16_t)(cy + hh), QG_TRANSPARENT, body);
    qg_box(s, (int16_t)(cx - hw), (int16_t)(cy - hh + r), (int16_t)(cx + hw), (int16_t)(cy + hh - r), QG_TRANSPARENT, body);
    qg_circle(s, (int16_t)(cx - hw + r), (int16_t)(cy - hh + r), r, QG_TRANSPARENT, body);
    qg_circle(s, (int16_t)(cx + hw - r), (int16_t)(cy - hh + r), r, QG_TRANSPARENT, body);
    qg_circle(s, (int16_t)(cx - hw + r), (int16_t)(cy + hh - r), r, QG_TRANSPARENT, body);
    qg_circle(s, (int16_t)(cx + hw - r), (int16_t)(cy + hh - r), r, QG_TRANSPARENT, body);

    if (squash < 0.4f) return;                    /* edge-on: no pips visible */

    /* Pip positions on a 3x3 grid: -1, 0, +1 across and down. */
    static const int8_t layout[7][6][2] = {
        {{0}},
        {{0, 0}},
        {{-1, -1}, {1, 1}},
        {{-1, -1}, {0, 0}, {1, 1}},
        {{-1, -1}, {1, -1}, {-1, 1}, {1, 1}},
        {{-1, -1}, {1, -1}, {0, 0}, {-1, 1}, {1, 1}},
        {{-1, -1}, {1, -1}, {-1, 0}, {1, 0}, {-1, 1}, {1, 1}},
    };
    int16_t step_x = (int16_t)(hw * 0.52f), step_y = (int16_t)(hh * 0.52f);
    int16_t pr = (int16_t)(size / 11), prx = (int16_t)(pr * squash);
    for (int i = 0; i < value; i++) {
        qg_ellipse(s, (int16_t)(cx + layout[value][i][0] * step_x), (int16_t)(cy + layout[value][i][1] * step_y),
                   prx < 1 ? 1 : prx, pr, QG_TRANSPARENT, pip);
    }
}

/* ========================================================================== */
/*  Screen B: table and histogram                                             */
/* ========================================================================== */

static void draw_table(qg_screen_t *s)
{
    /* Called for the whole table, and also inside small views to repair the
     * patches the dice just left. The view does the cutting.              */
    const int16_t w = qg_screen_width(s);
    qg_box(s, 0, 0, (int16_t)(w - 1), table_bottom, QG_TRANSPARENT, wood);
    qg_box(s, 8, 8, (int16_t)(w - 9), (int16_t)(table_bottom - 8), QG_TRANSPARENT, felt);
    qg_screen_set_line_style(s, 0xF0F0);
    qg_box(s, 16, 16, (int16_t)(w - 17), (int16_t)(table_bottom - 16), felt_dark, QG_TRANSPARENT);
    qg_screen_set_line_style(s, 0xFFFF);
}

static void draw_histogram(qg_screen_t *s)
{
    const int16_t w = qg_screen_width(s), h = qg_screen_height(s);
    char buf[40];

    /* Everything below the table: a view with its origin moved, so this
     * code draws in the panel's own coordinates and percentages.          */
    qg_view(s, 0, (int16_t)(table_bottom + 1), (int16_t)(w - 1), (int16_t)(h - 1), true);
    qg_cls(s, QG_BLACK);

    uint32_t most = 1, sum = 0;
    for (int t = 2; t <= 12; t++) { if (totals[t] > most) most = totals[t]; sum += totals[t] * (uint32_t)t; }
    snprintf(buf, sizeof buf, "{f:2}%lu roll%s, average %lu.%lu", (unsigned long)roll_count,
             roll_count == 1 ? "" : "s",
             (unsigned long)(roll_count ? sum / roll_count : 0),
             (unsigned long)(roll_count ? (sum * 10 / roll_count) % 10 : 0));
    qg_print_at(s, 6, 4, buf, QG_LIGHTGRAY, NULL);

    const int16_t pw = qg_view_width(s), ph = qg_view_height(s);
    const int16_t base = (int16_t)(ph - 18), top = 24, slot = (int16_t)((pw - 12) / 11);
    for (int t = 2; t <= 12; t++) {
        int16_t x = (int16_t)(6 + (t - 2) * slot);
        int16_t bar = (int16_t)((base - top) * totals[t] / most);
        qg_color_t c = (t == 7) ? QG_YELLOW : (t == 2 || t == 12) ? QG_LIGHTRED : QG_LIGHTCYAN;
        if (bar > 0) qg_box(s, (int16_t)(x + 2), (int16_t)(base - bar), (int16_t)(x + slot - 3), base, QG_TRANSPARENT, c);
        snprintf(buf, sizeof buf, "%d", t);
        qg_print_at(s, (int16_t)(x + (t < 10 ? slot / 2 - 3 : slot / 2 - 7)), (int16_t)(base + 3), buf, QG_WHITE, &font_small);
    }
    qg_view_reset(s);
}

/* ========================================================================== */
/*  The roll                                                                  */
/* ========================================================================== */

typedef struct { int16_t x, y, size; } die_pos_t;

/* Repair the table under a die's last position (its bounding box). */
static void erase_die(qg_screen_t *s, const die_pos_t *d)
{
    int16_t h = (int16_t)(d->size / 2 + 1);
    qg_view(s, (int16_t)(d->x - h), (int16_t)(d->y - h), (int16_t)(d->x + h), (int16_t)(d->y + h), false);
    draw_table(s);
    qg_view_reset(s);
}

static void roll(qg_screen_t *b, int *out1, int *out2)
{
    const int16_t w = qg_screen_width(b);
    const int16_t size = (int16_t)(qg_pct_r(b, 100) / 4);        /* dice: a quarter of the width */
    const int16_t floor_y = (int16_t)(table_bottom / 2 + 10);
    const int16_t target[2] = { (int16_t)(w * 32 / 100), (int16_t)(w * 68 / 100) };
    const int     final[2]  = { 1 + (int)board_random(6), 1 + (int)board_random(6) };

    die_pos_t pos[2] = { { (int16_t)-size, floor_y, size }, { (int16_t)(-size * 2), floor_y, size } };
    int face[2] = { 1 + (int)board_random(6), 1 + (int)board_random(6) };
    float spin[2] = { 0.0f, 1.3f };

    /* A fresh table: clears the LAST roll's dice and total. (Each frame below
     * only repairs where THIS roll's dice have been, so without this the
     * previous dice would sit there being chipped away by the new ones.)   */
    draw_table(b);

    const int frames = 48;
    for (int f = 0; f <= frames; f++) {
        float t = (float)f / frames;
        float ease = 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);     /* fast, then slow */
        for (int i = 0; i < 2; i++) erase_die(b, &pos[i]);

        for (int i = 0; i < 2; i++) {
            float start = (float)(-size * (i + 1));
            pos[i].x = (int16_t)(start + (target[i] - start) * ease);
            /* Bounces that shrink as the dice slow down. */
            float hop = fabsf(sinf(t * 3.14159265f * 3.0f + (float)i)) * (1.0f - t) * (float)(table_bottom / 3);
            pos[i].y = (int16_t)(floor_y - hop);
            /* Spin: squash follows a cosine; each time the die goes edge-on,
             * a new face comes round. On the last frame, the real result.   */
            float before = cosf(spin[i]);
            spin[i] += (1.0f - t) * 0.9f;
            if ((before > 0) != (cosf(spin[i]) > 0)) face[i] = 1 + (int)board_random(6);
            float squash = (f == frames) ? 1.0f : fabsf(cosf(spin[i]));
            if (f == frames) face[i] = final[i];
            draw_die(b, pos[i].x, pos[i].y, size, squash, face[i]);
        }
        qg_screen_flush(b);           /* only the patches around the dice go out */
        sleep_ms(16);
    }

    /* The verdict, big. */
    char buf[32];
    snprintf(buf, sizeof buf, "{s:2}%d", final[0] + final[1]);
    qg_print_box(b, 20, (int16_t)(table_bottom - 50), (int16_t)(w - 40), buf, QG_ALIGN_CENTER);
    qg_screen_flush(b);

    *out1 = final[0];
    *out2 = final[1];
}

/* ========================================================================== */
/*  Screen A: the log                                                         */
/* ========================================================================== */

static void log_roll(qg_screen_t *a, int d1, int d2)
{
    char buf[96];
    int total = d1 + d2;
    const char *colour = "WHITE", *note = "";
    if (total == 2)       { colour = "LIGHTRED";   note = "  {c:LIGHTRED}Snake eyes!"; }
    else if (total == 12) { colour = "LIGHTGREEN"; note = "  {c:LIGHTGREEN}Boxcars!"; }
    else if (d1 == d2)    { colour = "YELLOW";     note = "  {c:YELLOW}Doubles"; }
    else if (total == 7)  { colour = "LIGHTCYAN"; }

    /* Tabs line the columns up; markup colours the total. Past the bottom
     * of the screen, the log scrolls up by itself.                         */
    snprintf(buf, sizeof buf, "#%lu\t%d + %d\t{c:%s}%2d{c:}%s",
             (unsigned long)roll_count, d1, d2, colour, total, note);
    qg_println(a, buf);
}

/* ========================================================================== */
int main(void)
{
    board_init_two_fb(fb_b, sizeof fb_b);
    qg_screen_t *a = &screen_a, *b = &screen_b;

#if ROLL_BUTTON_PIN >= 0
    gpio_init(ROLL_BUTTON_PIN);
    gpio_set_dir(ROLL_BUTTON_PIN, GPIO_IN);
    gpio_pull_up(ROLL_BUTTON_PIN);             /* pressed = pulled to GND = 0 */
#endif

    /* Colours by RGB, looked up once. The standard palette has a 216-colour
     * cube, so these land on its nearest entries.                          */
    felt      = qg_color_from_rgb(20, 110, 50, b->palette);
    felt_dark = qg_color_from_rgb(10, 70, 30, b->palette);
    wood      = qg_color_from_rgb(110, 60, 20, b->palette);

    table_bottom = (int16_t)(qg_screen_height(b) * 62 / 100);
    qg_cls(b, QG_BLACK);
    draw_table(b);
    draw_histogram(b);
    qg_screen_flush(b);

    qg_cls(a, QG_BLACK);
    qg_screen_set_tab_width(a, 56);
    qg_locate(a, 6, 4);
    qg_println(a, "{f:1}Roll log");
    qg_println(a, "{f:2}{c:YELLOW}Roll\tDice\tTotal");

    while (true) {
#if ROLL_BUTTON_PIN >= 0
        while (gpio_get(ROLL_BUTTON_PIN)) sleep_ms(10);      /* wait for a press */
#endif
        int d1, d2;
        roll(b, &d1, &d2);
        roll_count++;
        totals[d1 + d2]++;
        log_roll(a, d1, d2);
        draw_histogram(b);
        qg_screen_flush(b);
#if ROLL_BUTTON_PIN < 0
        sleep_ms(AUTO_ROLL_MS);
#else
        while (!gpio_get(ROLL_BUTTON_PIN)) sleep_ms(10);     /* wait for release */
#endif
    }
}
