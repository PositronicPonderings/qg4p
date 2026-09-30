/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    layout.c
 * @brief   Example 4: one layout that fits any screen size and rotation,
 *          written in percentages, with a panel drawn inside a VIEW.
 *
 * Target: qg4p_layout.   Screens: A.   Expected: examples/expected/layout.png
 *
 * Every position here is a percentage (0 = first pixel, 100 = last), so the
 * same code fills a 240x320 screen upright, sideways, or a 320x480 one.
 * The screen turns every few seconds to prove it isn't cheating.
 *
 * The panel on the right uses qg_view() with move_origin = true: inside it,
 * (0,0) is the panel's corner and percentages measure the PANEL. So the
 * panel's drawing code has no idea where the panel is, and doesn't care.
 */
#include "pico/stdlib.h"
#include "board.h"

static void draw_panel(qg_screen_t *s)
{
    /* Everything here is relative to the panel, not the screen. */
    qg_cls(s, QG_DARKGRAY);
    qg_print_at(s, 3, 2, "{f:2}Status", QG_WHITE, NULL);
    qg_box_pct(s, 10, 30, 90, 45, QG_TRANSPARENT, QG_GREEN);      /* a health bar */
    qg_box_pct(s, 10, 55, 60, 70, QG_TRANSPARENT, QG_CYAN);       /* a mana bar   */
    qg_circle_pct(s, 50, 88, 10, QG_YELLOW, QG_TRANSPARENT);
}

static void draw_layout(qg_screen_t *s)
{
    qg_view_reset(s);
    qg_cls(s, QG_BLACK);
    qg_box_pct(s, 0, 0, 100, 10, QG_TRANSPARENT, QG_BLUE);         /* header */
    qg_print_at(s, 4, 2, "{f:1}Layout", QG_WHITE, NULL);

    qg_screen_set_line_width(s, 3);
    qg_circle_pct(s, 30, 45, 22, QG_WHITE, QG_RED);                /* main art */
    qg_screen_set_line_width(s, 1);

    qg_box_pct(s, 4, 80, 30, 95, QG_LIGHTGREEN, QG_GREEN);         /* buttons */
    qg_box_pct(s, 37, 80, 63, 95, QG_LIGHTCYAN, QG_CYAN);
    qg_box_pct(s, 70, 80, 96, 95, QG_LIGHTRED, QG_RED);

    /* The panel: a VIEW with its origin moved to the panel's corner. */
    qg_view_pct(s, 62, 18, 96, 72, true);
    draw_panel(s);
    qg_view_reset(s);                          /* always put the view back */
}

int main(void)
{
    board_init();
    for (int r = 0; ; r = (r + 1) % 4) {
        qg_screen_set_rotation(&screen_a, (qg_rotation_t)r);
        draw_layout(&screen_a);
        sleep_ms(3000);
    }
}
