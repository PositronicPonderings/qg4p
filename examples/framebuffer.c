/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    framebuffer.c
 * @brief   Example 9: the same bouncing ball, now flicker-free, because the
 *          screen draws into RAM and shows only finished frames.
 *
 * Target: qg4p_framebuffer.   Screens: A (as BUF8).
 * Expected: examples/expected/framebuffer.png
 *
 * Compare with animation_direct.c. The differences are:
 *   1. board_init_fb() instead of board_init(), with a framebuffer: one byte
 *      per pixel of RAM
 *   2. qg_screen_flush() after each frame, to show it
 * That's all. Every drawing call is identical; nothing reaches the glass
 * until the flush, so the "erased but not yet redrawn" moment is never seen.
 *
 * The flush only sends the rectangle that changed since the last one (here,
 * the patch around the old and new ball), which is why it's quick.
 */
#include <stdio.h>
#include "pico/stdlib.h"
#include "board.h"

#define R 12

static uint8_t fb[BOARD_A_WIDTH * BOARD_A_HEIGHT];   /* 75 KB of the Pico's 520 */

static void draw_background(qg_screen_t *s)
{
    qg_cls(s, QG_BLUE);
    for (int16_t x = 0; x < qg_screen_width(s); x = (int16_t)(x + 20))
        qg_line(s, x, 30, x, (int16_t)(qg_screen_height(s) - 1), QG_LIGHTBLUE);
    for (int16_t y = 30; y < qg_screen_height(s); y = (int16_t)(y + 20))
        qg_line(s, 0, y, (int16_t)(qg_screen_width(s) - 1), y, QG_LIGHTBLUE);
}

int main(void)
{
    board_init_fb(fb, sizeof fb);                       /* <-- difference 1 */
    qg_screen_t *s = &screen_a;
    const int16_t w = qg_screen_width(s), h = qg_screen_height(s);

    qg_cls(s, QG_BLACK);
    draw_background(s);
    qg_print_at(s, 4, 4, "BUF8: no flicker", QG_WHITE, NULL);
    qg_screen_set_text_bg(s, QG_BLACK);
    qg_screen_flush(s);

    int16_t x = 60, y = 100, vx = 3, vy = 2;
    char buf[24];
    uint64_t t0 = time_us_64();
    for (uint32_t frame = 0; ; frame++) {
        qg_view(s, (int16_t)(x - R), (int16_t)(y - R), (int16_t)(x + R), (int16_t)(y + R), false);
        draw_background(s);
        qg_view_reset(s);

        x = (int16_t)(x + vx); y = (int16_t)(y + vy);
        if (x < R || x > w - 1 - R) { vx = (int16_t)-vx; x = (int16_t)(x + 2 * vx); }
        if (y < 30 + R || y > h - 1 - R) { vy = (int16_t)-vy; y = (int16_t)(y + 2 * vy); }
        qg_circle(s, x, y, R, QG_WHITE, QG_RED);

        /* Frames per second, measured, shown in the corner. */
        if (frame % 30 == 29) {
            uint64_t us = time_us_64() - t0;
            snprintf(buf, sizeof buf, "%3lu fps", (unsigned long)(30000000ull / (us ? us : 1)));
            qg_print_at(s, 170, 4, buf, QG_YELLOW, &font_small);
            t0 = time_us_64();
        }
        qg_screen_flush(s);                          /* <-- difference 2 */
        sleep_ms(16);
    }
}
