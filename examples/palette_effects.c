/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    palette_effects.c
 * @brief   Example 10: animation without moving a pixel: rippling water and
 *          rising fire made by changing palette colours; plus a picture shown
 *          in its exact colours.
 *
 * Target: qg4p_palette_effects.   Screens: A (as BUF8).
 * Expected: examples/expected/palette_effects.png
 *
 * On a framebuffer screen each pixel stores a palette NUMBER. Draw the rings
 * once, each ring in its own palette entry; then every frame, shift which
 * colour each entry holds. At the next flush every ring changes colour, and
 * the eye sees motion. The drawing never changes: only 64 palette entries
 * per frame. (The Amiga and VGA crowd did this for waterfalls in 1990 and
 * felt very clever. They were.)
 *
 * Palette entries used here:
 *   16..141   the picture's own colours (qg_palette_load_image)
 *   160..191  fire         200..231  water
 */
#include "pico/stdlib.h"
#include "board.h"
#include "example_art.h"

static uint8_t fb[BOARD_A_WIDTH * BOARD_A_HEIGHT];

#define WATER 200
#define FIRE  160
#define N     32

int main(void)
{
    board_init_fb(fb, sizeof fb);
    qg_screen_t *s = &screen_a;
    const int16_t w = qg_screen_width(s);
    qg_cls(s, QG_BLACK);

    /* Water: concentric rings, one palette entry each. */
    for (int k = N - 1; k >= 0; k--) {
        qg_circle(s, w / 2, 85, (int16_t)(8 + k * 2), QG_TRANSPARENT, (qg_color_t)(WATER + k));
    }

    /* A picture in its EXACT colours: copy its palette into entries 16+,
     * which leaves the 16 named colours (used by text) alone.             */
    qg_image_t scene;
    qg_image_open(&scene, img_scene, img_scene_size, 0);
    qg_palette_load_image(s, &scene, 16);
    qg_image_draw_fit(s, &scene, 0, 160, w, 80, QG_ALIGN_CENTER);

    /* Fire: horizontal bands, bottom to top, one entry each. */
    for (int k = 0; k < N; k++) {
        int16_t y = (int16_t)(318 - k * 2 - 1);
        qg_box(s, 0, y, (int16_t)(w - 1), (int16_t)(y + 1), QG_TRANSPARENT, (qg_color_t)(FIRE + k));
    }
    qg_print_at(s, 4, 4, "{f:1}Palette magic", QG_WHITE, NULL);

    for (int phase = 0; ; phase++) {
        for (int k = 0; k < N; k++) {
            /* Water: a travelling wave of light and dark blue. */
            int v = (k * 8 + phase * 6) % 64;  v = v < 32 ? v : 63 - v;       /* 0..31..0 */
            qg_palette_set(s, (qg_color_t)(WATER + k), (uint8_t)(10 + v * 3), (uint8_t)(60 + v * 5), (uint8_t)(140 + v * 3));
            /* Fire: heat flickers upward; hotter at the bottom. */
            int heat = 255 - k * 7 + (int)((k * 37 + phase * 11) % 40) - 20;
            if (heat < 0) heat = 0;
            if (heat > 255) heat = 255;
            qg_palette_set(s, (qg_color_t)(FIRE + k), (uint8_t)heat, (uint8_t)(heat * heat / 400), (uint8_t)(heat > 230 ? (heat - 230) * 4 : 0));
        }
        qg_screen_flush(s);         /* a palette change sends the whole screen */
        sleep_ms(30);
    }
}
