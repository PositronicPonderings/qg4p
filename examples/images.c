/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    images.c
 * @brief   Example 5: images compiled into the program: drawn 1:1, scaled,
 *          fitted into a box, and with see-through backgrounds.
 *
 * Target: qg4p_images.   Screens: A.   Expected: examples/expected/images.png
 *
 * Where the images come from: examples/art/make_art.py draws them, and
 * tools/img2bmp8.py turns PNGs into 8-bit BMPs inside a C file
 * (example_art.c). For your own:
 *     python3 tools/img2bmp8.py mine.png --out mine.bmp --c-array img_mine
 * then add mine.c to your build and declare  extern const uint8_t img_mine[];
 * (Or skip the C files entirely: see the asset_pack example.)
 */
#include "pico/stdlib.h"
#include "board.h"
#include "example_art.h"

int main(void)
{
    board_init();
    qg_screen_t *s = &screen_a;
    qg_image_t logo, scene, star, ship;

    /* Opening only reads the header: quick, and nothing is copied. The
     * images stay in flash. Transparent ones get QG_IMAGE_TRANSPARENT
     * (img2bmp8.py tells you which ones need it).                         */
    qg_image_open(&logo,  img_logo,  img_logo_size,  0);
    qg_image_open(&scene, img_scene, img_scene_size, 0);
    qg_image_open(&star,  img_star,  img_star_size,  QG_IMAGE_TRANSPARENT);
    qg_image_open(&ship,  img_ship,  img_ship_size,  QG_IMAGE_TRANSPARENT);

    qg_cls(s, QG_BLACK);
    qg_image_draw(s, &logo, 0, 0);                                /* 1:1 */

    /* Fitted into a box: as big as possible without changing its shape. */
    qg_box(s, 4, 52, 235, 171, QG_DARKGRAY, QG_TRANSPARENT);
    qg_image_draw_fit(s, &scene, 5, 53, 230, 118, QG_ALIGN_CENTER);

    /* Transparency: the stars sit on the scene, not in black squares. */
    qg_image_draw(s, &star, 20, 70);
    qg_image_draw_scaled(s, &star, 180, 60, 48, 48);              /* 1.5x */

    /* Pixel art scales crisply by whole numbers: 1x, 2x, 4x, 6x. */
    qg_image_draw(s, &ship, 10, 200);
    qg_image_draw_scaled(s, &ship, 36, 192, 32, 32);
    qg_image_draw_scaled(s, &ship, 80, 176, 64, 64);
    qg_image_draw_scaled(s, &ship, 150, 170, 96, 96);

    /* Stretched: width and height scale separately. */
    qg_image_draw_scaled(s, &star, 10, 270, 120, 40);
    qg_print_at(s, 140, 280, "{f:2}stretched", QG_LIGHTGRAY, NULL);

    while (true) tight_loop_contents();
}
