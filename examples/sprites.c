/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    sprites.c
 * @brief   Example 12: sprites with GET and PUT: stamping see-through copies,
 *          moving with XOR, and moving "properly" by saving what's underneath.
 *
 * Target: qg4p_sprites.   Screens: A (as BUF8).   Expected: examples/expected/sprites.png
 *
 * Three techniques, from the golden age of home computers:
 *   1. STAMP: PUT with TRANSPARENT copies the sprite, skipping index 255.
 *   2. XOR:   PUT with XOR draws it; the same PUT again erases it, exactly.
 *             No saving, no redrawing. The catch: over a busy background the
 *             colours scramble. (That shimmer was a feature in 1983.)
 *   3. SAVE-UNDER: GET the background where the sprite will go, PUT the
 *             sprite, and later PUT the saved background back. Correct
 *             colours, one extra buffer.
 */
#include "pico/stdlib.h"
#include "board.h"
#include "example_art.h"

static uint8_t fb[BOARD_A_WIDTH * BOARD_A_HEIGHT];

#define SZ 48                                   /* the ship, drawn at 3x */
static uint8_t ship_black[QG_BLOCK_BYTES(SZ, SZ)];   /* black around it: for XOR  */
static uint8_t ship_clear[QG_BLOCK_BYTES(SZ, SZ)];   /* 255 around it: to stamp   */
static uint8_t under[QG_BLOCK_BYTES(SZ, SZ)];        /* saved background          */

static void background(qg_screen_t *s)
{
    qg_cls(s, QG_BLACK);
    for (int k = 0; k < 250; k++) {                              /* stars */
        qg_pset(s, (int16_t)board_random(240), (int16_t)(30 + board_random(260)),
                (qg_color_t)(232 + board_random(23)));
    }
    for (int16_t x = 0; x < 240; x = (int16_t)(x + 16))          /* a busy strip */
        qg_box(s, x, 280, (int16_t)(x + 15), 319, QG_TRANSPARENT, (qg_color_t)(16 + (x / 16) * 13));
}

int main(void)
{
    board_init_fb(fb, sizeof fb);
    qg_screen_t *s = &screen_a;

    /* Make the sprites: draw the ship on black, GET it, then make a copy
     * with the black turned into 255 (see-through) for stamping.          */
    qg_image_t ship;
    qg_image_open(&ship, img_ship, img_ship_size, QG_IMAGE_TRANSPARENT);
    qg_cls(s, QG_BLACK);
    qg_image_draw_scaled(s, &ship, 0, 0, SZ, SZ);
    qg_get(s, 0, 0, SZ - 1, SZ - 1, ship_black, sizeof ship_black);
    for (uint32_t i = 0; i < sizeof ship_black; i++) {
        ship_clear[i] = (i >= 4 && ship_black[i] == QG_BLACK) ? 255 : ship_black[i];
    }

    background(s);
    qg_print_at(s, 4, 4, "{f:1}Sprites", QG_WHITE, NULL);
    for (int k = 0; k < 3; k++) {                                /* 1. stamps */
        qg_put(s, (int16_t)(20 + k * 75), 40, ship_clear, QG_PUT_TRANSPARENT);
    }
    qg_screen_flush(s);

    for (int16_t x = -SZ; ; x = (int16_t)(x + 2)) {
        if (x > 240) x = -SZ;
        /* 2. XOR ship, over the busy strip: draw, show, erase. */
        qg_put(s, x, 276, ship_black, QG_PUT_XOR);
        /* 3. Save-under ship, over the stars: save, draw, show, restore.
         *    It bounces between the edges, because GET has to stay entirely
         *    on the screen (a partial save-under would restore garbage).   */
        int16_t span = 240 - SZ, p = (int16_t)((x + SZ) % (2 * span));
        int16_t x2 = (int16_t)(p < span ? span - p : p - span);   /* 0..span..0 */
        qg_get(s, x2, 150, (int16_t)(x2 + SZ - 1), 150 + SZ - 1, under, sizeof under);
        qg_put(s, x2, 150, ship_clear, QG_PUT_TRANSPARENT);

        qg_screen_flush(s);
        sleep_ms(20);

        qg_put(s, x, 276, ship_black, QG_PUT_XOR);           /* XOR again: gone */
        qg_put(s, x2, 150, under, QG_PUT_PSET);                 /* background back */
    }
}
