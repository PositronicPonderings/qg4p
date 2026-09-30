/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    paint.c
 * @brief   Example 11: a colouring book. Draw outlines, then flood-fill the
 *          spaces with qg_paint(), and read colours back with qg_point().
 *
 * Target: qg4p_paint.   Screens: A (as BUF8).   Expected: examples/expected/paint.png
 *
 * PAINT needs to read pixels back, which only a framebuffer can do.
 * The outlines must be closed: paint escapes through a gap of one pixel,
 * exactly like water, and floods everything. (You WILL do this at least
 * once. Everyone does. It's a rite of passage.)
 */
#include <stdio.h>
#include "pico/stdlib.h"
#include "board.h"

static uint8_t fb[BOARD_A_WIDTH * BOARD_A_HEIGHT];

static void outlines(qg_screen_t *s)
{
    qg_cls(s, QG_BLACK);
    qg_line(s, 0, 260, 239, 260, QG_WHITE);                     /* ground     */
    qg_box(s, 40, 150, 160, 259, QG_WHITE, QG_TRANSPARENT);     /* walls      */
    qg_line(s, 30, 150, 100, 90, QG_WHITE);                     /* roof       */
    qg_line(s, 100, 90, 170, 150, QG_WHITE);
    qg_line(s, 30, 150, 170, 150, QG_WHITE);
    qg_box(s, 85, 205, 115, 259, QG_WHITE, QG_TRANSPARENT);     /* door       */
    qg_box(s, 55, 170, 80, 195, QG_WHITE, QG_TRANSPARENT);      /* windows    */
    qg_box(s, 120, 170, 145, 195, QG_WHITE, QG_TRANSPARENT);
    qg_line(s, 67, 170, 67, 195, QG_WHITE); qg_line(s, 55, 182, 80, 182, QG_WHITE);
    qg_line(s, 132, 170, 132, 195, QG_WHITE); qg_line(s, 120, 182, 145, 182, QG_WHITE);
    qg_circle(s, 200, 60, 25, QG_WHITE, QG_TRANSPARENT);        /* sun        */
    qg_box(s, 195, 215, 205, 259, QG_WHITE, QG_TRANSPARENT);    /* tree trunk */
    qg_circle(s, 200, 195, 22, QG_WHITE, QG_TRANSPARENT);       /* tree top   */
}

int main(void)
{
    board_init_fb(fb, sizeof fb);
    qg_screen_t *s = &screen_a;

    /* Where to pour each colour (any point inside the space will do). */
    static const struct { int16_t x, y; qg_color_t c; } fills[] = {
        { 100, 120, QG_LIGHTRED }, { 60, 230, QG_BROWN },  { 100, 230, QG_RED },
        { 60, 175, QG_LIGHTCYAN }, { 75, 190, QG_CYAN },   { 125, 175, QG_LIGHTCYAN },
        { 140, 190, QG_CYAN },     { 200, 60, QG_YELLOW }, { 200, 195, QG_LIGHTGREEN },
        { 200, 240, QG_BROWN },    { 10, 290, QG_GREEN },  { 10, 10, QG_LIGHTBLUE },
    };

    while (true) {
        outlines(s);
        qg_print_at(s, 4, 4, "PAINT", QG_YELLOW, &font_title);
        qg_screen_flush(s);
        sleep_ms(800);

        for (unsigned i = 0; i < sizeof fills / sizeof fills[0]; i++) {
            qg_paint(s, fills[i].x, fills[i].y, fills[i].c, QG_WHITE);   /* stop at white */
            qg_screen_flush(s);
            sleep_ms(250);
        }

        /* The sky fill stopped only at WHITE, so it painted over the title.
         * Put the title back on top.                                       */
        qg_print_at(s, 4, 4, "PAINT", QG_YELLOW, &font_title);

        /* POINT: what colour is the door? */
        char buf[40];
        const char *name = qg_color_name(qg_point(s, 100, 230));
        snprintf(buf, sizeof buf, "The door is %s.", name ? name : "a mystery");
        qg_print_at(s, 4, 300, buf, QG_BLACK, &font_small);
        qg_screen_flush(s);
        sleep_ms(4000);
    }
}
