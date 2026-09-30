/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    dashboard.c
 * @brief   Example 13: an instrument panel: arc gauges with moving needles,
 *          readouts that update in place, and a bar meter.
 *
 * Target: qg4p_dashboard.   Screens: A.   Expected: examples/expected/dashboard.png
 *
 * All on a DIRECT screen, and nothing flickers much, because each frame only
 * touches what moved: the needle is erased by drawing it again in the
 * background colour, readouts are opaque text that overwrites itself, and
 * the bar only draws the part that changed.
 */
#include <math.h>
#include <stdio.h>
#include "pico/stdlib.h"
#include "board.h"

typedef struct {
    int16_t cx, cy, r;           /* centre and track radius           */
    float   lo, hi;              /* the value range                   */
    float   angle;               /* where the needle is now (degrees) */
    const char *label;
} gauge_t;

/* Values map onto a 240-degree sweep: 210 (lower left) to -30 (lower right). */
static float to_angle(const gauge_t *g, float v)
{
    float t = (v - g->lo) / (g->hi - g->lo);
    t = t < 0 ? 0 : (t > 1 ? 1 : t);
    return 210.0f - 240.0f * t;
}

static void needle(qg_screen_t *s, const gauge_t *g, float deg, qg_color_t c)
{
    float a = deg * 3.14159265f / 180.0f;
    int16_t len = (int16_t)(g->r - 14);
    qg_screen_set_line_width(s, 3);
    qg_line(s, g->cx, g->cy, (int16_t)(g->cx + len * cosf(a)), (int16_t)(g->cy - len * sinf(a)), c);
    qg_screen_set_line_width(s, 1);
    qg_circle(s, g->cx, g->cy, 5, QG_WHITE, QG_DARKGRAY);   /* the hub, on top */
}

static void gauge_face(qg_screen_t *s, gauge_t *g)
{
    qg_screen_set_line_width(s, 8);
    qg_arc(s, g->cx, g->cy, g->r, g->r, 90, 210, QG_GREEN);      /* safe     */
    qg_arc(s, g->cx, g->cy, g->r, g->r, 10, 90, QG_YELLOW);      /* careful  */
    qg_arc(s, g->cx, g->cy, g->r, g->r, -30, 10, QG_RED);        /* too much */
    qg_screen_set_line_width(s, 1);
    /* Labels go BELOW the arc's open ends, where the needle never reaches:
     * erasing the needle paints black over anything it crossed.           */
    qg_print_at(s, (int16_t)(g->cx - 20), (int16_t)(g->cy + g->r / 2 + 4), g->label, QG_LIGHTGRAY, &font_small);
    g->angle = 210.0f;
    needle(s, g, g->angle, QG_WHITE);
}

static void gauge_set(qg_screen_t *s, gauge_t *g, float v)
{
    float a = to_angle(g, v);
    if (fabsf(a - g->angle) < 0.5f) return;      /* didn't really move: skip */
    needle(s, g, g->angle, QG_BLACK);           /* erase: same line, bg colour */
    needle(s, g, a, QG_WHITE);
    g->angle = a;
}

int main(void)
{
    board_init();
    qg_screen_t *s = &screen_a;
    qg_cls(s, QG_BLACK);
    qg_print_at(s, 4, 2, "{f:1}Dashboard", QG_DEFAULT, NULL);

    gauge_t speed = { 120, 115, 80, 0, 120, 0, "SPEED" };
    gauge_t temp  = { 60, 245, 45, 40, 110, 0, "TEMP" };
    gauge_face(s, &speed);
    gauge_face(s, &temp);

    const int16_t bx = 130, by = 222, bw = 100, bh = 20;          /* fuel bar */
    qg_box(s, bx, by, (int16_t)(bx + bw - 1), (int16_t)(by + bh - 1), QG_WHITE, QG_TRANSPARENT);
    qg_print_at(s, bx, (int16_t)(by + bh + 4), "FUEL", QG_LIGHTGRAY, &font_small);
    int16_t fuel_px = bw - 4;
    qg_box(s, (int16_t)(bx + 2), (int16_t)(by + 2), (int16_t)(bx + 1 + fuel_px), (int16_t)(by + bh - 3), QG_TRANSPARENT, QG_LIGHTGREEN);

    qg_screen_set_text_bg(s, QG_BLACK);
    char buf[24];
    for (uint32_t t = 0; ; t++) {
        float ft = (float)t * 0.03f;
        float v_speed = 62.0f + 50.0f * sinf(ft) + 6.0f * sinf(ft * 3.7f);   /* 6..118 */
        float v_temp  = 75.0f + 30.0f * sinf(ft * 0.4f);
        gauge_set(s, &speed, v_speed);
        gauge_set(s, &temp, v_temp);

        /* MONOSPACED for numbers that change: every character, spaces
         * included, is the same width, so " 60" exactly covers "118".
         * In a proportional font a space is narrower than a digit, and the
         * old number's edges would be left peeking out.                     */
        snprintf(buf, sizeof buf, "{f:2}{s:2}%3d", (int)v_speed);
        qg_print_at(s, 99, 176, buf, QG_WHITE, NULL);                /* under the gauge */
        snprintf(buf, sizeof buf, "%3d C", (int)v_temp);
        qg_print_at(s, 38, 290, buf, QG_WHITE, &font_small);

        /* The fuel bar only ever shrinks: blank the part that emptied. */
        int16_t want = (int16_t)((bw - 4) * (1.0f - (float)(t % 3000) / 3000.0f));
        if (want < fuel_px) {
            qg_box(s, (int16_t)(bx + 2 + want), (int16_t)(by + 2), (int16_t)(bx + 1 + fuel_px),
                   (int16_t)(by + bh - 3), QG_TRANSPARENT, QG_BLACK);
            fuel_px = want;
        } else if (want > fuel_px) {                                /* refuelled */
            qg_box(s, (int16_t)(bx + 2), (int16_t)(by + 2), (int16_t)(bx + 1 + want),
                   (int16_t)(by + bh - 3), QG_TRANSPARENT, QG_LIGHTGREEN);
            fuel_px = want;
        }
        sleep_ms(20);
    }
}
