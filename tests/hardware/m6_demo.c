/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    m6_demo.c
 * @brief   Milestone 6 test: images (8-bit BMP, RLE8), transparency,
 *          scaling and fitting.
 *
 * Built by tests/hardware/CMakeLists.txt as its own target (qg4p_m6.uf2).
 *
 * PAGES
 *   1  1:1        the landscape and banner at their own size; the d20 over a
 *                 checkerboard with and without QG_IMAGE_TRANSPARENT
 *   2  Scaling    pixel art at x2, x4, x8; a stretched and a squashed d20;
 *                 the landscape at half size
 *   3  Fit        the landscape fitted into a wide, a tall and a square box,
 *                 aligned left, centre and right
 *   4  Speed      timed draws, and images hanging off the screen edges
 */
#include <stdio.h>
#include "pico/stdlib.h"
#include "qg4p.h"
#include "demo_setup.h"
#include "demo_images.h"

static qg_font_t f_body = QG_FONT_INIT(qg_font_sans_16, QG_DEFAULT, 1);
static qg_font_t f_mono = QG_FONT_INIT(qg_font_mono_12, QG_DEFAULT, 1);

static qg_screen_t *const screens[2] = { &scr_a, &scr_b };
static const char   *const names[2]   = { "A", "B" };

/* Opened once at start-up. Opening only reads the header: cheap. */
static qg_image_t d20, d20_opaque, potion, banner, landscape;

static void open_or_report(qg_image_t *img, const uint8_t *data, uint32_t size,
                           uint8_t flags, const char *name)
{
    qg_err_t err = qg_image_open(img, data, size, flags);
    printf("  %-10s %3d x %3d  %s  %lu bytes  %s\n", name, img->width, img->height,
           img->rle ? "RLE8 " : "plain", (unsigned long)size,
           err == QG_OK ? "OK" : "FAILED");
}

static void checkerboard(qg_screen_t *s, int16_t x, int16_t y, int16_t w, int16_t h, int16_t sq)
{
    for (int16_t j = 0; j < h; j = (int16_t)(j + sq)) {
        for (int16_t i = 0; i < w; i = (int16_t)(i + sq)) {
            qg_color_t c = (((i + j) / sq) & 1) ? QG_DARKGRAY : QG_LIGHTGRAY;
            qg_box(s, (int16_t)(x + i), (int16_t)(y + j),
                    (int16_t)(x + i + sq - 1), (int16_t)(y + j + sq - 1), QG_TRANSPARENT, c);
        }
    }
}

static void label(qg_screen_t *s, int16_t x, int16_t y, const char *t)
{
    qg_print_at(s, x, y, t, QG_YELLOW, &f_mono);
}

/* ========================================================================== */
static void page_one_to_one(qg_screen_t *s)
{
    qg_cls(s, QG_BLACK);
    qg_image_draw(s, &banner, 0, 0);
    qg_image_draw(s, &landscape, 0, 50);

    int16_t y = 216;
    label(s, 4, y, "with flag");
    label(s, 124, y, "without");
    checkerboard(s, 4, (int16_t)(y + 16), 96, 80, 16);
    checkerboard(s, 124, (int16_t)(y + 16), 96, 80, 16);
    qg_image_draw(s, &d20, 20, (int16_t)(y + 24));          /* see-through   */
    qg_image_draw(s, &d20_opaque, 140, (int16_t)(y + 24));  /* magenta shows */
}

static void page_scaling(qg_screen_t *s)
{
    qg_cls(s, QG_BLACK);
    label(s, 4, 2, "pixel art: x1 x2 x4 | x8");
    qg_image_draw(s, &potion, 4, 20);                           /* x1 */
    qg_image_draw_scaled(s, &potion, 4, 40, 32, 32);            /* x2 */
    qg_image_draw_scaled(s, &potion, 4, 76, 64, 64);            /* x4 */
    qg_image_draw_scaled(s, &potion, 100, 20, 128, 128);        /* x8 */

    label(s, 4, 150, "stretched / squashed");
    qg_image_draw_scaled(s, &d20, 4, 164, 128, 48);             /* wide */
    qg_image_draw_scaled(s, &d20, 150, 164, 32, 96);            /* tall */

    label(s, 4, 218, "half size");
    qg_image_draw_scaled(s, &landscape, 4, 234, 120, 80);
}

static void fit_box(qg_screen_t *s, int16_t x, int16_t y, int16_t w, int16_t h, qg_align_t a)
{
    qg_box(s, (int16_t)(x - 1), (int16_t)(y - 1), (int16_t)(x + w), (int16_t)(y + h),
            QG_WHITE, QG_BLUE);
    qg_image_draw_fit(s, &landscape, x, y, w, h, a);
}

static void page_fit(qg_screen_t *s)
{
    const int16_t w = qg_screen_width(s);
    qg_cls(s, QG_BLACK);
    label(s, 4, 2, "wide box: left / centre / right");
    fit_box(s, 4, 18, (int16_t)(w - 8), 60, QG_ALIGN_LEFT);
    fit_box(s, 4, 84, (int16_t)(w - 8), 60, QG_ALIGN_CENTER);
    fit_box(s, 4, 150, (int16_t)(w - 8), 60, QG_ALIGN_RIGHT);
    label(s, 4, 216, "tall box        square box");
    fit_box(s, 4, 232, 60, 80, QG_ALIGN_CENTER);
    fit_box(s, 100, 232, 80, 80, QG_ALIGN_CENTER);
}

static void page_speed(qg_screen_t *s, const char *name)
{
    const int16_t w = qg_screen_width(s), h = qg_screen_height(s);
    uint64_t t;

    qg_cls(s, QG_BLACK);
    t = time_us_64(); qg_image_draw(s, &landscape, 0, 0);
    printf("  %-6s landscape 1:1 (240x160):       %6lu us\n", name, (unsigned long)(time_us_64() - t));

    t = time_us_64(); qg_image_draw_scaled(s, &landscape, 0, 0, w, h);
    printf("  %-6s landscape full screen:         %6lu us\n", name, (unsigned long)(time_us_64() - t));

    t = time_us_64(); qg_image_draw_scaled(s, &banner, 0, 0, w, 48);
    printf("  %-6s banner, screen width:          %6lu us\n", name, (unsigned long)(time_us_64() - t));

    t = time_us_64(); qg_image_draw(s, &d20, (int16_t)(w / 2 - 32), (int16_t)(h / 2 - 32));
    printf("  %-6s d20 1:1, transparent:          %6lu us\n", name, (unsigned long)(time_us_64() - t));

    t = time_us_64(); qg_image_draw_scaled(s, &d20, (int16_t)(w / 2 - 64), (int16_t)(h / 2 + 40), 128, 128);
    printf("  %-6s d20 x2, transparent:           %6lu us\n", name, (unsigned long)(time_us_64() - t));

    /* Images hanging off every edge: must be cut off cleanly. */
    qg_image_draw(s, &d20, -32, -20);
    qg_image_draw(s, &d20, (int16_t)(w - 30), -30);
    qg_image_draw(s, &d20, -40, (int16_t)(h - 30));
    qg_image_draw(s, &d20, (int16_t)(w - 24), (int16_t)(h - 24));
}

/* ========================================================================== */
int main(void)
{
    demo_setup("Dice Roller qg4p - Milestone 6");

    printf("\nOpening images:\n");
    open_or_report(&d20,        img_d20,       img_d20_size,       QG_IMAGE_TRANSPARENT, "d20");
    open_or_report(&d20_opaque, img_d20,       img_d20_size,       0,                     "d20 (no flag)");
    open_or_report(&potion,     img_potion,    img_potion_size,    QG_IMAGE_TRANSPARENT, "potion");
    open_or_report(&banner,     img_banner,    img_banner_size,    0,                     "banner");
    open_or_report(&landscape,  img_landscape, img_landscape_size, 0,                     "landscape");

    for (int i = 0; i < 2; i++) qg_screen_set_font(screens[i], 0, &f_body);

    while (true) {
        printf("\n--- Page 1: images at 1:1 ---\n");
        printf("  Look for: banner and landscape; left d20 over the checkerboard,\n"
               "  right d20 on a MAGENTA square (the flag was left off).\n");
        for (int i = 0; i < 2; i++) page_one_to_one(screens[i]);
        sleep_ms(5000);

        printf("\n--- Page 2: scaling ---\n");
        printf("  Look for: crisp, blocky potions; a wide and a tall d20.\n");
        for (int i = 0; i < 2; i++) page_scaling(screens[i]);
        sleep_ms(5000);

        printf("\n--- Page 3: fit ---\n");
        printf("  Look for: the landscape never distorted, left/centre/right in\n"
               "  the wide boxes, centred in the tall and square ones.\n");
        for (int i = 0; i < 2; i++) page_fit(screens[i]);
        sleep_ms(5000);

        printf("\n--- Page 4: speed and clipping ---\n");
        for (int i = 0; i < 2; i++) page_speed(screens[i], names[i]);
        sleep_ms(4000);
    }
}
