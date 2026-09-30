/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/*
 * Template for the manual's examples (tests/host/doc_examples.py fills it
 * in). Every example in docs/manual runs inside doc_example() below, with:
 *   scr                      a ready 240x320 screen (a framebuffer screen
 *                            for examples tagged "buf8"), cleared to black
 *   font_body/title/small    the three built-in fonts, in slots 0, 1, 2
 *   img_star/ship/scene/logo open qg_image_t's of the example art
 * and afterwards the screen is saved as the example's picture.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "qg4p.h"
#include "qg_internal.h"
#include "qg_asset.h"
#include "example_art.h"

#define W 240
#define H 320
qg_screen_t scr;
qg_font_t font_body  = QG_FONT_INIT(qg_font_sans_16,      QG_DEFAULT, 1);
qg_font_t font_title = QG_FONT_INIT(qg_font_sans_bold_24, QG_YELLOW,  1);
qg_font_t font_small = QG_FONT_INIT(qg_font_mono_12,      QG_DEFAULT, 1);
qg_image_t img_star_i, img_ship_i, img_scene_i, img_logo_i;
#define star   img_star_i
#define ship   img_ship_i
#define scene  img_scene_i
#define logo   img_logo_i

static uint16_t panel[H * W];
static uint8_t  fb[W * H];
static long oob;
static void dff(qg_screen_t *s, int16_t x, int16_t y, int16_t w, int16_t h, qg_color_t c)
{ if (x < 0 || y < 0 || x + w > s->width || y + h > s->height) { oob++; return; }
  const uint16_t *out = qg_int_out(s);
  for (int j = y; j < y + h; j++) for (int i = x; i < x + w; i++) panel[j * s->width + i] = out[c]; }
static void dwr(qg_screen_t *s, int16_t x, int16_t y, int16_t w, int16_t h, const uint16_t *p)
{ if (x < 0 || y < 0 || x + w > s->width || y + h > s->height) { oob++; return; }
  for (int j = 0; j < h; j++) for (int i = 0; i < w; i++) panel[(y + j) * s->width + x + i] = p[j * w + i]; }
static const qg_backend_t direct = { .name = "DIRECT", .fill_rect = dff, .write_rgb565 = dwr };

/* framebuffer flushes, and the hardware calls examples might make */
static int wx0, wx1, wcx, wcy;
void qg_hal_begin(qg_hal_device_t *d) { (void)d; }
void qg_hal_end(qg_hal_device_t *d) { (void)d; }
void qg_hal_write_cmd(qg_hal_device_t *d, uint8_t c) { (void)d; (void)c; }
void qg_hal_write_data(qg_hal_device_t *d, const uint8_t *p, size_t n) { (void)d; (void)p; (void)n; }
void qg_driver_set_window(qg_hal_device_t *d, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{ (void)d; (void)y1; wx0 = x0; wx1 = x1; wcx = x0; wcy = y0; }
void qg_hal_stream_begin(qg_hal_device_t *d) { (void)d; }
void qg_hal_stream_pixels(qg_hal_device_t *d, const uint16_t *p, uint32_t n)
{ (void)d; for (uint32_t i = 0; i < n; i++) { panel[wcy * scr.width + wcx] = p[i]; if (++wcx > wx1) { wcx = wx0; wcy++; } } }
void qg_hal_stream_end(qg_hal_device_t *d) { (void)d; }
void qg_screen_flush(qg_screen_t *s) { if (s->backend->flush) s->backend->flush(s); }
void qg_screen_flush_all(qg_screen_t *s) { if (!s->fb) return; qg_int_dirty_all(s); s->backend->flush(s); }
void qg_screen_set_line_width(qg_screen_t *s, uint8_t w) { s->line_width = w < 1 ? 1 : w; }
void qg_screen_set_colors(qg_screen_t *s, qg_color_t fg, qg_color_t bg) { if (fg <= 254) s->fg_color = fg; if (bg <= 254) s->bg_color = bg; }
void qg_screen_set_brightness(qg_screen_t *s, uint8_t p) { s->brightness = p > 100 ? 100 : p; }
void qg_screen_backlight(qg_screen_t *s, bool on) { (void)s; (void)on; }
qg_err_t qg_screen_set_rotation(qg_screen_t *s, qg_rotation_t r)
{ s->rotation = r; if (r & 1) { s->width = H; s->height = W; } else { s->width = W; s->height = H; }
  qg_view_reset(s); qg_int_dirty_all(s); return QG_OK; }
void sleep_ms(uint32_t ms) { (void)ms; }
uint64_t time_us_64(void) { static uint64_t t; return t += 50; }
void tight_loop_contents(void) {}
bool stdio_init_all(void) { return true; }
int getchar_timeout_us(uint32_t us) { (void)us; return PICO_ERROR_TIMEOUT; }
char __flash_binary_end;

#ifdef DOC_PACK
/* Examples tagged "pack": the examples' asset pack, from a file (on a Pico
 * it's in flash; qg_asset_init is renamed to this for these examples).     */
qg_asset_err_t doc_asset_init(void)
{
    static uint8_t pack[1 << 20];
    FILE *f = fopen("pack/assets.bin", "rb");
    if (!f) return QG_ASSET_ERR_NO_PACK;
    size_t n = fread(pack, 1, sizeof pack, f);
    fclose(f);
    (void)n;
    return qg_asset_init_at(pack);
}
#endif

static void doc_example(void)
{
/*SNIPPET*/
}

int main(int argc, char **argv)
{
    static qg_text_line_t history[QG_TEXT_HISTORY_LINES];
    memset(&scr, 0, sizeof scr);
    scr.width = W; scr.height = H; scr.ready = true;
    scr.fg_color = QG_WHITE; scr.bg_color = QG_BLACK; scr.line_width = 1; scr.line_style = 0xFFFF;
    scr.text_bg = QG_TRANSPARENT; scr.tab_width = QG_TAB_WIDTH; scr.wrap = true; scr.scroll = true;
    scr.brightness = 100; scr.hist = history; scr.hist_cap = QG_TEXT_HISTORY_LINES;
    qg_palette_copy_standard(scr.palette);
    qg_view_reset(&scr);
#ifdef DOC_BUF8
    scr.fb = fb; scr.backend = &qg_backend_buf8;
#else
    (void)fb; scr.backend = &direct;
#endif
    qg_screen_set_font(&scr, 0, &font_body);
    qg_screen_set_font(&scr, 1, &font_title);
    qg_screen_set_font(&scr, 2, &font_small);
    qg_image_open(&img_star_i,  img_star,  img_star_size,  QG_IMAGE_TRANSPARENT);
    qg_image_open(&img_ship_i,  img_ship,  img_ship_size,  QG_IMAGE_TRANSPARENT);
    qg_image_open(&img_scene_i, img_scene, img_scene_size, 0);
    qg_image_open(&img_logo_i,  img_logo,  img_logo_size,  0);
    qg_cls(&scr, QG_BLACK);
    qg_screen_flush_all(&scr);

    doc_example();
    qg_screen_flush(&scr);

    FILE *f = fopen(argc > 1 ? argv[1] : "out.ppm", "wb");
    fprintf(f, "P6 %d %d 255\n", scr.width, scr.height);
    for (int i = 0; i < scr.width * scr.height; i++) {
        uint16_t p = panel[i];
        uint8_t c[3] = { (uint8_t)(((p >> 11) & 31) << 3), (uint8_t)(((p >> 5) & 63) << 2), (uint8_t)((p & 31) << 3) };
        fwrite(c, 1, 3, f);
    }
    fclose(f);
    if (oob) fprintf(stderr, "%ld out-of-bounds writes\n", oob);
    return oob != 0;
}
