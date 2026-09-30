/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    text.c
 * @brief   Example 3: fonts, the print cursor, markup, wrapping, alignment,
 *          tabs, and a counter that updates in place.
 *
 * Target: qg4p_text.   Screens: A.   Expected: examples/expected/text.png
 *
 * Markup cheat sheet (full list in qg4p/qg_text.h):
 *   {c:RED} or {c:200}   colour         {c:}  back to default
 *   {f:1}                font slot      {f:}  back to default
 *   {s:2}                scale 1..4     {s:}  back to default
 *   {x:120}              jump to column 120
 *   {{                   a literal brace, because "{" now means business
 */
#include <stdio.h>
#include "pico/stdlib.h"
#include "board.h"

int main(void)
{
    board_init();
    qg_screen_t *s = &screen_a;
    qg_cls(s, QG_BLACK);

    /* The print cursor, QuickBasic style: LOCATE, then PRINT. New lines go
     * back to the x you gave qg_locate(), so indented blocks just work.    */
    qg_locate(s, 6, 4);
    qg_println(s, "{f:1}Text");
    qg_println(s, "Colours: {c:LIGHTRED}red{c:}, {c:LIGHTGREEN}green{c:}, {c:14}yellow");
    qg_println(s, "Sizes: small {s:2}BIG{s:} small");
    qg_println(s, "Fonts: {f:2}mono{f:} and {f:1}bold{f:}");

    /* Long text wraps at word boundaries. qg_print_box() wraps in a column
     * of any width, and aligns each line within it.                        */
    qg_box(s, 6, 124, 233, 208, QG_DARKGRAY, QG_TRANSPARENT);
    qg_print_box(s, 12, 128, 216,
                 "This paragraph wraps to fit its box, and each line is "
                 "{c:YELLOW}centred{c:}. Nobody had to count characters.",
                 QG_ALIGN_CENTER);

    /* Tabs line up columns; the tab stops are every 40 px by default.      */
    qg_locate(s, 6, 216);
    qg_println(s, "{f:2}{c:YELLOW}Item\tQty\tCost");
    qg_println(s, "{f:2}Rope\t2\t5 gp");
    qg_println(s, "{f:2}Torch\t10\t1 gp");

    /* A counter that updates in place. Opaque text paints its own
     * background, so each new number simply covers the old one: no erasing,
     * no flicker. Two details make it work:
     *   - "%5lu" pads to a fixed number of characters, and
     *   - the MONOSPACED font ({f:2}) makes every character, spaces
     *     included, the same width, so a fixed count is a fixed width.
     * In a proportional font a space is narrower than a digit, "   42" is
     * narrower than "12345", and the old number's edges peek out.          */
    qg_print_at(s, 6, 284, "Uptime:", QG_LIGHTGRAY, NULL);
    qg_screen_set_text_bg(s, QG_BLACK);
    char buf[32];
    for (uint32_t n = 0; ; n++) {
        snprintf(buf, sizeof buf, "{f:2}{s:2}%5lu.%lu s", (unsigned long)(n / 10), (unsigned long)(n % 10));
        qg_print_at(s, 84, 278, buf, QG_LIGHTCYAN, NULL);
        sleep_ms(100);
    }
}
