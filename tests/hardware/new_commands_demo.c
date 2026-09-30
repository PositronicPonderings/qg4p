/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    new_commands_demo.c
 * @brief   Test of the commands added for the public release: VIEW, LINE
 *          styles, PRESET, CSRLIN/POS (screen A, DIRECT) and GET/PUT
 *          (screen B, framebuffer).
 *
 * Built by tests/hardware/CMakeLists.txt as its own target
 * (qg4p_new_commands.uf2).
 *
 * PAGES
 *   1  VIEW      a clip-only view cutting shapes off, and a moved-origin
 *                panel laid out in percentages of itself
 *   2  Styles    dashed, dotted and dash-dot lines and boxes, thin and thick
 *   3  GET/PUT   (screen B) a sprite captured with GET, stamped with PSET and
 *                TRANSPARENT, then slid across a busy background with XOR,
 *                which restores the background exactly as it moves
 *   4  PRESET    a dotted line erased point by point; CSRLIN/POS readouts
 */
#include <stdio.h>
#include "pico/stdlib.h"
#include "qg4p.h"
#include "demo_setup.h"
#include "demo_images.h"

static uint8_t fb_b[320 * 480];

static qg_font_t f_body  = QG_FONT_INIT(qg_font_sans_16,      QG_DEFAULT, 1);
static qg_font_t f_title = QG_FONT_INIT(qg_font_sans_bold_24, QG_YELLOW,  1);
static qg_font_t f_mono  = QG_FONT_INIT(qg_font_mono_12,      QG_DEFAULT, 1);

static qg_image_t d20;

/* ========================================================================== */
static void page_view(void)
{
    qg_screen_t *s = &scr_a;
    const int16_t w = qg_screen_width(s), h = qg_screen_height(s);

    printf("\n--- Page 1: VIEW ---\n");
    qg_view_reset(s);
    qg_cls(s, QG_BLACK);
    qg_print_at(s, 4, 2, "{f:1}VIEW", QG_DEFAULT, NULL);

    /* Left: a clip-only view. Coordinates stay screen coordinates; the
     * circle and lines are simply cut off at the view's edges.            */
    qg_box(s, 9, 39, (int16_t)(w / 2 - 6), 181, QG_DARKGRAY, QG_TRANSPARENT);   /* frame, outside the view */
    qg_view(s, 10, 40, (int16_t)(w / 2 - 7), 180, false);
    qg_cls(s, QG_BLUE);                                    /* clears just the view */
    qg_circle(s, 10, 110, 60, QG_WHITE, QG_RED);
    for (int16_t k = 0; k < 8; k++) qg_line(s, 0, (int16_t)(20 + k * 25), w, (int16_t)(k * 25), QG_YELLOW);
    qg_view_reset(s);

    /* Right: a moved-origin view. (0,0) is the panel's corner and the
     * percentages measure the PANEL, so this code doesn't know where it is. */
    qg_box(s, (int16_t)(w / 2 + 4), 39, (int16_t)(w - 6), 181, QG_DARKGRAY, QG_TRANSPARENT);
    qg_view(s, (int16_t)(w / 2 + 5), 40, (int16_t)(w - 7), 180, true);
    qg_cls(s, QG_DARKGRAY);
    qg_circle_pct(s, 50, 40, 30, QG_WHITE, QG_GREEN);
    qg_box_pct(s, 10, 80, 90, 95, QG_TRANSPARENT, QG_CYAN);
    qg_print_at(s, 2, 2, "Panel", QG_BLACK, &f_mono);
    qg_view_reset(s);

    /* Bottom: text in a view wraps at the view's edge. */
    qg_view(s, 10, 195, (int16_t)(w - 11), (int16_t)(h - 10), true);
    qg_cls(s, QG_BROWN);
    qg_print_at(s, 4, 4, "Text in a view wraps at the view's own edge, and is "
                         "{c:YELLOW}cut off{c:} at its bottom instead of scrolling. "
                         "This sentence keeps going to prove the point, point, point.",
                QG_WHITE, NULL);
    qg_view_reset(s);
}

/* ========================================================================== */
static void page_styles(void)
{
    qg_screen_t *s = &scr_a;
    const int16_t w = qg_screen_width(s);
    static const uint16_t styles[] = { 0xF0F0, 0xAAAA, 0xFF18, 0xFFF0, 0xCCCC };
    static const char *names[]     = { "F0F0 dashed", "AAAA dotted", "FF18 dash-dot", "FFF0 long", "CCCC short" };

    printf("\n--- Page 2: LINE styles ---\n");
    qg_cls(s, QG_BLACK);
    qg_print_at(s, 4, 2, "{f:1}LINE styles", QG_DEFAULT, NULL);
    for (int i = 0; i < 5; i++) {
        int16_t y = (int16_t)(40 + i * 24);
        qg_screen_set_line_style(s, styles[i]);
        qg_screen_set_line_width(s, 1);
        qg_line(s, 4, y, (int16_t)(w / 2 - 4), y, QG_WHITE);
        qg_screen_set_line_width(s, 4);
        qg_line(s, (int16_t)(w / 2 + 4), y, (int16_t)(w - 4), y, QG_LIGHTCYAN);
        qg_print_at(s, 4, (int16_t)(y + 3), names[i], QG_DARKGRAY, &f_mono);
    }
    qg_screen_set_line_width(s, 1);
    qg_screen_set_line_style(s, 0xF0F0);
    qg_line(s, 10, 170, (int16_t)(w - 10), 230, QG_YELLOW);                  /* diagonal */
    qg_box(s, 10, 245, (int16_t)(w / 2 - 5), 310, QG_WHITE, QG_BLUE);        /* styled box */
    qg_screen_set_line_width(s, 3);
    qg_screen_set_line_style(s, 0xFF00);
    qg_box(s, (int16_t)(w / 2 + 5), 245, (int16_t)(w - 10), 310, QG_LIGHTRED, QG_TRANSPARENT);
    qg_screen_set_line_width(s, 1);
    qg_screen_set_line_style(s, 0xFFFF);                                     /* solid again */
}

/* ========================================================================== */
#define SPRITE 64

static void page_getput(void)
{
    qg_screen_t *s = &scr_b;
    const int16_t w = qg_screen_width(s), h = qg_screen_height(s);
    static uint8_t sprite[QG_BLOCK_BYTES(SPRITE, SPRITE)];     /* black background */
    static uint8_t cutout[QG_BLOCK_BYTES(SPRITE, SPRITE)];     /* see-through one  */

    printf("\n--- Page 3: GET/PUT (screen B, framebuffer) ---\n");

    /* Draw the d20 on black and GET it. Two versions are useful:
     *   sprite  keeps black (index 0) around the die. For XOR that's ideal:
     *           anything XOR 0 is unchanged, so the corners are invisible.
     *   cutout  has that black turned into 255, which TRANSPARENT skips.   */
    qg_cls(s, QG_BLACK);
    qg_image_draw(s, &d20, 0, 0);
    qg_err_t e = qg_get(s, 0, 0, SPRITE - 1, SPRITE - 1, sprite, sizeof sprite);
    for (int i = 0; i < (int)sizeof sprite; i++) {
        cutout[i] = (i >= 4 && sprite[i] == QG_BLACK) ? 255 : sprite[i];
    }
    printf("  qg_get: %s\n", e == QG_OK ? "OK" : "error");

    /* A busy background, so the modes show their differences. */
    qg_cls(s, QG_BLACK);
    for (int16_t y = 0; y < h; y = (int16_t)(y + 16)) {
        for (int16_t x = 0; x < w; x = (int16_t)(x + 16)) {
            qg_box(s, x, y, (int16_t)(x + 15), (int16_t)(y + 15), QG_TRANSPARENT,
                   (qg_color_t)(16 + ((x / 16 + y / 16) * 7) % 216));
        }
    }
    qg_print_at(s, 8, 8, "{f:1}GET / PUT", QG_WHITE, NULL);
    qg_put(s, 10, 50, sprite, QG_PUT_PSET);          /* the whole square */
    qg_put(s, 90, 50, cutout, QG_PUT_TRANSPARENT);   /* just the die     */
    qg_put(s, 170, 50, sprite, QG_PUT_XOR);          /* XOR-ed in        */
    qg_print_at(s, 10, 118, "PSET     TRANSP.  XOR", QG_WHITE, &f_mono);
    qg_screen_flush(s);
    sleep_ms(2500);

    /* Slide a sprite across with XOR: PUT once to show it, PUT again at the
     * same place to erase it, which restores the background EXACTLY. No need
     * to save what was underneath: that's why XOR was the classic method.  */
    uint64_t t0 = time_us_64();
    int frames = 0;
    for (int16_t x = -SPRITE; x < w; x = (int16_t)(x + 3), frames++) {
        int16_t y = (int16_t)(h / 2 + 40);
        qg_put(s, x, y, sprite, QG_PUT_XOR);        /* show  */
        qg_screen_flush(s);
        sleep_ms(8);
        qg_put(s, x, y, sprite, QG_PUT_XOR);        /* erase */
    }
    qg_screen_flush(s);
    printf("  XOR slide: %d frames, %lu us per frame (incl. 8 ms pause)\n",
           frames, (unsigned long)((time_us_64() - t0) / (uint64_t)frames));
}

/* ========================================================================== */
static void page_preset(void)
{
    qg_screen_t *s = &scr_a;
    const int16_t w = qg_screen_width(s);
    char buf[48];

    printf("\n--- Page 4: PRESET, CSRLIN/POS ---\n");
    qg_screen_set_colors(s, QG_WHITE, QG_BLACK);
    qg_cls(s, QG_BLACK);
    qg_print_at(s, 4, 2, "{f:1}PRESET", QG_DEFAULT, NULL);

    for (int16_t x = 10; x < w - 10; x = (int16_t)(x + 3)) qg_pset(s, x, 60, QG_LIGHTGREEN);
    sleep_ms(800);
    for (int16_t x = 10; x < w - 10; x = (int16_t)(x + 3)) {   /* erase them, one by one */
        qg_preset(s, x, 60, QG_DEFAULT);
        sleep_ms(15);
    }

    qg_locate(s, 10, 90);
    qg_print(s, "Cursor here: ");
    snprintf(buf, sizeof buf, "POS=%d CSRLIN=%d", qg_pos(s), qg_csrlin(s));
    qg_println(s, buf);
    qg_print(s, "Next line: ");
    snprintf(buf, sizeof buf, "POS=%d CSRLIN=%d", qg_pos(s), qg_csrlin(s));
    qg_println(s, buf);
    printf("  %s\n", buf);
}

/* ========================================================================== */
int main(void)
{
    demo_setup_ex("QG4P - new commands", fb_b, sizeof fb_b);
    qg_screen_t *screens[2] = { &scr_a, &scr_b };
    for (int i = 0; i < 2; i++) {
        qg_screen_set_font(screens[i], 0, &f_body);
        qg_screen_set_font(screens[i], 1, &f_title);
        qg_screen_set_font(screens[i], 2, &f_mono);
    }
    qg_image_open(&d20, img_d20, img_d20_size, QG_IMAGE_TRANSPARENT);

    while (true) {
        page_view();    sleep_ms(4000);
        page_styles();  sleep_ms(4000);
        page_getput();  sleep_ms(1500);
        page_preset();  sleep_ms(3000);
    }
}
