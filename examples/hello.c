/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    hello.c
 * @brief   Example 1: the smallest program that puts something on a screen.
 *
 * Target: qg4p_hello.   Screens: A.   Expected: examples/expected/hello.png
 *
 * Three steps, and two of them are one line:
 *   1. board_init() wakes up the bus and screen A (board.c has the details;
 *      board.h has the settings for your wiring)
 *   2. draw
 *   3. there is no step 3. Enjoy the rest of your evening.
 */
#include "pico/stdlib.h"
#include "board.h"

int main(void)
{
    board_init();                           /* screen A, drawing direct */

    qg_screen_t *s = &screen_a;
    qg_cls(s, QG_BLUE);
    qg_screen_set_line_width(s, 3);
    qg_circle_pct(s, 50, 40, 30, QG_WHITE, QG_RED);          /* centred, any size */
    qg_print_align(s, 250, "Hello, {c:YELLOW}Pico{c:}!", QG_ALIGN_CENTER);

    while (true) tight_loop_contents();     /* nothing left to do but admire it */
}
