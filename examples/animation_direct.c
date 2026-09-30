/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    animation_direct.c
 * @brief   Example 8: smooth-ish motion on a DIRECT screen (no framebuffer),
 *          by redrawing only what changed.
 *
 * Target: qg4p_animation_direct.   Screens: A.
 * Expected: examples/expected/animation_direct.png
 *
 * On a DIRECT screen every drawing call shows up immediately. To move the
 * ball we must erase it (repaint the background where it was) and draw it
 * again. The trick is to repaint ONLY that small patch:
 *
 *   qg_view() around the old ball, redraw the whole background (the view
 *   throws away everything outside the patch), qg_view_reset(), draw the
 *   ball in its new place.
 *
 * The background code doesn't need to know anything about patches: the
 * view does the cutting. There's still a split second between "erased" and
 * "drawn" that the eye can catch as flicker; example 9 (framebuffer) makes
 * it go away completely with two extra lines.
 */
#include <stdio.h>
#include "pico/stdlib.h"
#include "board.h"

#define R 12                                   /* ball radius */

static void draw_background(qg_screen_t *s)
{
    qg_cls(s, QG_BLUE);                         /* respects the view: clears the patch */
    for (int16_t x = 0; x < qg_screen_width(s); x = (int16_t)(x + 20))
        qg_line(s, x, 30, x, (int16_t)(qg_screen_height(s) - 1), QG_LIGHTBLUE);
    for (int16_t y = 30; y < qg_screen_height(s); y = (int16_t)(y + 20))
        qg_line(s, 0, y, (int16_t)(qg_screen_width(s) - 1), y, QG_LIGHTBLUE);
}

int main(void)
{
    board_init();
    qg_screen_t *s = &screen_a;
    const int16_t w = qg_screen_width(s), h = qg_screen_height(s);

    qg_cls(s, QG_BLACK);
    draw_background(s);
    qg_print_at(s, 4, 4, "DIRECT: patch repair", QG_WHITE, NULL);
    qg_screen_set_text_bg(s, QG_BLACK);        /* the counter overwrites itself */

    int16_t x = 60, y = 100, vx = 3, vy = 2;
    char buf[24];
    for (uint32_t frame = 0; ; frame++) {
        /* 1. Repair the background where the ball WAS. */
        qg_view(s, (int16_t)(x - R), (int16_t)(y - R), (int16_t)(x + R), (int16_t)(y + R), false);
        draw_background(s);
        qg_view_reset(s);

        /* 2. Move, bouncing off the edges (and off the title strip). */
        x = (int16_t)(x + vx); y = (int16_t)(y + vy);
        if (x < R || x > w - 1 - R) { vx = (int16_t)-vx; x = (int16_t)(x + 2 * vx); }
        if (y < 30 + R || y > h - 1 - R) { vy = (int16_t)-vy; y = (int16_t)(y + 2 * vy); }

        /* 3. Draw it where it IS. */
        qg_circle(s, x, y, R, QG_WHITE, QG_RED);

        snprintf(buf, sizeof buf, "%6lu", (unsigned long)frame);
        qg_print_at(s, 170, 4, buf, QG_YELLOW, &font_small);
        sleep_ms(16);
    }
}
