/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    shapes.c
 * @brief   Example 2: every drawing primitive, on one screen.
 *
 * Target: qg4p_shapes.   Screens: A.   Expected: examples/expected/shapes.png
 *
 * Rows, top to bottom:
 *   lines        thin, thick, and styled (dashed, dotted)
 *   boxes        outline, filled, both at once, thick
 *   circles      outline, filled, both, and an ellipse
 *   arcs         a gauge-like arc and a pie-ish ring
 *   points       a sprinkle of PSETs
 *
 * The rule for outline-and-fill shapes: pass QG_TRANSPARENT for the part you
 * don't want. Outline only: fill = QG_TRANSPARENT. Fill only: stroke =
 * QG_TRANSPARENT. Both: pass both. That's it; that's the whole trick.
 */
#include "pico/stdlib.h"
#include "board.h"

int main(void)
{
    board_init();
    qg_screen_t *s = &screen_a;
    qg_cls(s, QG_BLACK);
    qg_print_at(s, 4, 2, "{f:1}Shapes", QG_DEFAULT, NULL);

    /* --- Lines: thin, thick, dashed, dotted ------------------------------ */
    qg_line(s, 10, 40, 110, 40, QG_WHITE);                        /* 1 px    */
    qg_screen_set_line_width(s, 5);
    qg_line(s, 130, 40, 230, 40, QG_LIGHTCYAN);                   /* 5 px    */
    qg_screen_set_line_width(s, 1);
    qg_screen_set_line_style(s, 0xF0F0);                          /* dashed  */
    qg_line(s, 10, 55, 110, 55, QG_YELLOW);
    qg_screen_set_line_style(s, 0xAAAA);                          /* dotted  */
    qg_line(s, 130, 55, 230, 55, QG_LIGHTGREEN);
    qg_screen_set_line_style(s, 0xFFFF);                          /* solid again, or everything after this is dotted too */

    /* --- Boxes: corners in any order ------------------------------------- */
    qg_box(s, 10, 70, 60, 110, QG_WHITE, QG_TRANSPARENT);         /* outline */
    qg_box(s, 70, 70, 120, 110, QG_TRANSPARENT, QG_BLUE);         /* filled  */
    qg_box(s, 130, 70, 180, 110, QG_YELLOW, QG_RED);              /* both    */
    qg_screen_set_line_width(s, 4);
    qg_box(s, 190, 70, 230, 110, QG_LIGHTMAGENTA, QG_TRANSPARENT);/* thick: grows inward */
    qg_screen_set_line_width(s, 1);

    /* --- Circles and an ellipse ------------------------------------------ */
    qg_circle(s, 35, 150, 25, QG_WHITE, QG_TRANSPARENT);
    qg_circle(s, 95, 150, 25, QG_TRANSPARENT, QG_GREEN);
    qg_circle(s, 155, 150, 25, QG_LIGHTCYAN, QG_BLUE);
    qg_ellipse(s, 210, 150, 22, 12, QG_YELLOW, QG_BROWN);

    /* --- Arcs: angles in degrees, 0 = 3 o'clock, counter-clockwise ------- */
    qg_screen_set_line_width(s, 8);
    qg_arc(s, 60, 230, 45, 45, -30, 210, QG_DARKGRAY);            /* a gauge track */
    qg_arc(s, 60, 230, 45, 45, 120, 210, QG_LIGHTRED);            /* ...partly full */
    qg_screen_set_line_width(s, 3);
    for (int k = 0; k < 6; k++) {                                 /* a ring of slices */
        qg_arc(s, 175, 230, 40, 40, (int16_t)(k * 60 + 5), (int16_t)(k * 60 + 55),
               (qg_color_t)(QG_LIGHTBLUE + k));
    }
    qg_screen_set_line_width(s, 1);

    /* --- Points: the humblest primitive gets the last word --------------- */
    for (int k = 0; k < 300; k++) {
        qg_pset(s, (int16_t)(10 + board_random(220)), (int16_t)(285 + board_random(30)),
                (qg_color_t)(QG_LIGHTBLUE + board_random(7)));
    }

    while (true) tight_loop_contents();
}
