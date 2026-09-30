/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    m4_demo.c
 * @brief   Milestone 4 test: fonts and printing.
 *
 * Built by tests/hardware/CMakeLists.txt as its own target (qg4p_m4.uf2).
 *
 * PAGES
 *   1  Fonts      The three built-in fonts, scaling, symbols, and what a
 *                 missing character looks like
 *   2  Opaque     Text on a banner (transparent), then a fast-changing
 *                 counter drawn two ways, timed: erase-and-redraw versus
 *                 opaque text
 *   3  Cursor     LOCATE / PRINT style output: a roll log with margins,
 *                 print_at() for a status corner, and a centred result
 */
#include <stdio.h>
#include "pico/stdlib.h"
#include "qg4p.h"
#include "demo_setup.h"

/* Fonts: data + default colour + scale. Static, because screens keep
 * pointers to them.                                                         */
static qg_font_t f_mono   = QG_FONT_INIT(qg_font_mono_12,      QG_DEFAULT,   1);
static qg_font_t f_sans   = QG_FONT_INIT(qg_font_sans_16,      QG_DEFAULT,   1);
static qg_font_t f_title  = QG_FONT_INIT(qg_font_sans_bold_24, QG_YELLOW,    1);
static qg_font_t f_sans2  = QG_FONT_INIT(qg_font_sans_16,      QG_LIGHTCYAN, 2);
static qg_font_t f_big    = QG_FONT_INIT(qg_font_sans_bold_24, QG_WHITE,     2);

static qg_screen_t *const screens[2] = { &scr_a, &scr_b };
static const char   *const names[2]   = { "A", "B" };

/* A tiny random number generator for dice rolls (not for real games!). */
static uint32_t rng_state = 12345;
static int roll(int sides)
{
    rng_state = rng_state * 1103515245u + 12345u;
    return (int)((rng_state >> 16) % (uint32_t)sides) + 1;
}

/* ========================================================================== */
/*  Page 1: fonts                                                             */
/* ========================================================================== */
static void page_fonts(qg_screen_t *s)
{
    qg_cls(s, QG_BLACK);
    qg_screen_set_text_bg(s, QG_TRANSPARENT);

    int16_t y = 4;
    qg_print_at(s, 6, y, "Fonts", QG_DEFAULT, &f_title);        /* yellow: font colour */
    y = (int16_t)(y + qg_font_line_height(&f_title) + 4);

    qg_print_at(s, 6, y, "Mono 12: The quick brown fox", QG_DEFAULT, &f_mono);
    y = (int16_t)(y + qg_font_line_height(&f_mono));
    qg_print_at(s, 6, y, "0123456789 !?@#%&*()[]{{}", QG_LIGHTGREEN, &f_mono);
    y = (int16_t)(y + qg_font_line_height(&f_mono) + 6);

    qg_print_at(s, 6, y, "Sans 16: jumps over", QG_DEFAULT, &f_sans);
    y = (int16_t)(y + qg_font_line_height(&f_sans));
    qg_print_at(s, 6, y, "the lazy dog. gjpqy", QG_DEFAULT, &f_sans);
    y = (int16_t)(y + qg_font_line_height(&f_sans) + 6);

    qg_print_at(s, 6, y, "Bold 24", QG_LIGHTRED, &f_title);
    y = (int16_t)(y + qg_font_line_height(&f_title));
    qg_print_at(s, 6, y, "2d6 ± 3 × 2 °", QG_WHITE, &f_title);
    y = (int16_t)(y + qg_font_line_height(&f_title) + 6);

    qg_print_at(s, 6, y, "Scale 2", QG_DEFAULT, &f_sans2);
    y = (int16_t)(y + qg_font_line_height(&f_sans2) + 2);

    /* "€" isn't in these fonts, so it prints as "?". */
    qg_print_at(s, 6, y, "Missing: \xE2\x82\xAC -> ?", QG_DARKGRAY, &f_sans);
}

/* ========================================================================== */
/*  Page 2: transparent and opaque text                                       */
/* ========================================================================== */
#define COUNTER_STEPS 200

static void page_opaque(qg_screen_t *s, const char *name)
{
    const int16_t w = qg_screen_width(s);
    char buf[16];

    qg_cls(s, QG_BLACK);

    /* Transparent text over a shape: draw the banner, then the words. */
    qg_screen_set_text_bg(s, QG_TRANSPARENT);
    qg_box(s, 0, 0, (int16_t)(w - 1), 40, QG_TRANSPARENT, QG_BLUE);
    qg_print_at(s, 10, 6, "Critical hit!", QG_WHITE, &f_title);

    qg_print_at(s, 6, 52, "Erase + redraw:", QG_DEFAULT, &f_sans);
    qg_print_at(s, 6, 112, "Opaque text:", QG_DEFAULT, &f_sans);

    /* Method 1: transparent text can't cover its own old digits, so each
     * update must first clear the old number with a box.                    */
    const int16_t box_w = (int16_t)(qg_font_line_height(&f_big) * 3);
    uint64_t t0 = time_us_64();
    for (int i = 0; i <= COUNTER_STEPS; i++) {
        snprintf(buf, sizeof buf, "%3d", i);
        qg_box(s, 10, 72, (int16_t)(10 + box_w), (int16_t)(72 + qg_font_line_height(&f_sans2) - 1),
                QG_TRANSPARENT, QG_BLACK);
        qg_print_at(s, 10, 72, buf, QG_LIGHTCYAN, &f_sans2);
    }
    uint64_t t_erase = time_us_64() - t0;

    /* Method 2: opaque text paints each character's whole cell, so the new
     * digits simply replace the old ones. "%3d" keeps the width constant,
     * so nothing is left behind when the number gets shorter.               */
    qg_screen_set_text_bg(s, QG_BLACK);
    t0 = time_us_64();
    for (int i = 0; i <= COUNTER_STEPS; i++) {
        snprintf(buf, sizeof buf, "%3d", i);
        qg_print_at(s, 10, 132, buf, QG_LIGHTCYAN, &f_sans2);
    }
    uint64_t t_opaque = time_us_64() - t0;
    qg_screen_set_text_bg(s, QG_TRANSPARENT);

    printf("  %-6s erase+redraw: %lu us per update   opaque: %lu us per update\n",
           name, (unsigned long)(t_erase / (COUNTER_STEPS + 1)),
           (unsigned long)(t_opaque / (COUNTER_STEPS + 1)));
}

/* ========================================================================== */
/*  Page 3: the print cursor                                                  */
/* ========================================================================== */
static void page_cursor(qg_screen_t *s)
{
    const int16_t w = qg_screen_width(s), h = qg_screen_height(s);
    char buf[32];

    qg_cls(s, QG_BLACK);
    qg_screen_set_text_bg(s, QG_TRANSPARENT);
    qg_screen_set_font(s, 0, &f_sans);

    /* Status corner: print_at() never moves the print cursor. */
    qg_print_at(s, (int16_t)(w - 80), 4, "HP 12/20", QG_LIGHTGREEN, &f_mono);

    /* LOCATE then PRINT. New lines return to the x given to qg_locate(). */
    qg_screen_set_colors(s, QG_YELLOW, QG_DEFAULT);
    qg_locate(s, 8, 4);
    qg_println(s, "Roll log");

    qg_locate(s, 20, s->cursor_y);            /* indent the list */
    for (int i = 0; i < 5; i++) {
        int r = roll(20);
        qg_screen_set_colors(s, QG_LIGHTGRAY, QG_DEFAULT);
        snprintf(buf, sizeof buf, "d20 #%d: ", i + 1);
        qg_print(s, buf);                     /* cursor stays on this line */

        qg_screen_set_colors(s, (r == 20) ? QG_LIGHTGREEN : (r == 1) ? QG_LIGHTRED
                                                                        : QG_WHITE,
                              QG_DEFAULT);
        snprintf(buf, sizeof buf, "%d", r);
        qg_println(s, buf);                   /* ...then a new line at x = 20 */
    }

    /* A "\n" inside the text also returns to the margin. */
    qg_screen_set_colors(s, QG_CYAN, QG_DEFAULT);
    qg_println(s, "Two lines\nfrom one call");
    qg_screen_set_colors(s, QG_WHITE, QG_DEFAULT);

    /* A big result, centred with qg_text_measure(). */
    int16_t tw, th;
    snprintf(buf, sizeof buf, "%d", roll(20));
    qg_text_measure(s, buf, &f_big, 0, &tw, &th);
    int16_t bx = (int16_t)((w - tw) / 2), by = (int16_t)(h - th - 20);

    qg_screen_set_line_width(s, 3);
    qg_box(s, (int16_t)(bx - 14), (int16_t)(by - 6), (int16_t)(bx + tw + 13),
            (int16_t)(by + th + 5), QG_YELLOW, QG_DARKGRAY);
    qg_screen_set_line_width(s, 1);
    qg_print_at(s, bx, by, buf, QG_DEFAULT, &f_big);
}

/* ========================================================================== */
int main(void)
{
    demo_setup("Dice Roller qg4p - Milestone 4");

    for (int i = 0; i < 2; i++) {
        qg_screen_set_font(screens[i], 0, &f_sans);
    }

    while (true) {
        printf("\n--- Page 1: fonts ---\n");
        printf("  Look for: three fonts, scale 2, the symbols ° ± ×, a '?' for\n"
               "  the missing euro sign, descenders (g j p q y) below the line.\n");
        for (int i = 0; i < 2; i++) {
            uint64_t t0 = time_us_64();
            page_fonts(screens[i]);
            printf("  %-6s drawn in %lu us\n", names[i], (unsigned long)(time_us_64() - t0));
        }
        sleep_ms(5000);

        printf("\n--- Page 2: transparent and opaque text ---\n");
        printf("  Look for: white text on the blue banner; both counters\n"
               "  counting to 200 cleanly (the opaque one should be faster).\n");
        for (int i = 0; i < 2; i++) {
            page_opaque(screens[i], names[i]);
        }
        sleep_ms(3000);

        printf("\n--- Page 3: print cursor ---\n");
        printf("  Look for: an indented roll log (20s green, 1s red), 'Two lines'\n"
               "  aligned with the list, HP in the corner, a centred big number.\n");
        for (int i = 0; i < 2; i++) {
            page_cursor(screens[i]);
        }
        sleep_ms(5000);
    }
}
