/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    two_screens.c
 * @brief   Example 6: two screens on one SPI bus, each with its own chip,
 *          speed and backlight.
 *
 * Target: qg4p_two_screens.   Screens: A and B.
 * Expected: examples/expected/two_screens_a.png and _b.png
 *
 * The two screens share SCK, MOSI, DC and RST; each has its own CS and
 * backlight wire (docs/WIRING.md). Every drawing call names its screen, so
 * the library knows which CS to pull. You never select a screen yourself.
 */
#include <stdio.h>
#include "pico/stdlib.h"
#include "board.h"

static void describe(qg_screen_t *s, const char *name, qg_color_t bg)
{
    char buf[64];
    qg_cls(s, bg);
    qg_locate(s, 8, 8);
    snprintf(buf, sizeof buf, "{f:1}Screen %s", name);
    qg_println(s, buf);
    snprintf(buf, sizeof buf, "%s, %d x %d", s->drv->name, qg_screen_width(s), qg_screen_height(s));
    qg_println(s, buf);
    snprintf(buf, sizeof buf, "SPI %lu.%lu MHz", (unsigned long)(s->dev.hz_actual / 1000000u),
             (unsigned long)(s->dev.hz_actual / 100000u % 10u));
    qg_println(s, buf);
    qg_screen_set_line_width(s, 4);
    qg_circle_pct(s, 50, 60, 25, QG_WHITE, QG_TRANSPARENT);
    qg_screen_set_line_width(s, 1);
}

int main(void)
{
    board_init_two();                 /* both screens, both DIRECT */
    describe(&screen_a, "A", QG_BLUE);
    describe(&screen_b, "B", QG_RED);

    /* Breathe the backlights in turn, independently: each screen has its
     * own PWM brightness, 0..100 %.                                        */
    char buf[24];
    for (int t = 0; ; t++) {
        int a = 50 + 50 * ((t % 100) < 50 ? (t % 50) : (50 - t % 50)) / 50;
        int b = 150 - a;
        qg_screen_set_brightness(&screen_a, (uint8_t)a);
        qg_screen_set_brightness(&screen_b, (uint8_t)(b > 100 ? 100 : b));

        qg_screen_set_text_bg(&screen_a, QG_BLUE);
        snprintf(buf, sizeof buf, "{f:2}{s:2}%3d%%", a);   /* mono: see text.c */
        qg_print_at(&screen_a, 8, 240, buf, QG_YELLOW, NULL);
        qg_screen_set_text_bg(&screen_b, QG_RED);
        snprintf(buf, sizeof buf, "{f:2}{s:2}%3d%%", b > 100 ? 100 : b);
        qg_print_at(&screen_b, 8, 400, buf, QG_YELLOW, NULL);
        sleep_ms(40);
    }
}
