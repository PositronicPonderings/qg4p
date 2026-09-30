/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/*
 * Tests for the commands added for the public release: VIEW, GET/PUT (all
 * six modes), LINE styles, PRESET, CSRLIN/POS and palette entry 255.
 * Every check compares against an independent expectation: the same scene
 * drawn without a view, byte arithmetic, or the pattern bits themselves.
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "qg4p.h"
#include "qg_internal.h"
#include "demo_images.h"

#define W 240
#define H 320
static int fails = 0;
static void check(int ok, const char *what) { printf("%s  %s\n", ok ? "PASS" : "FAIL", what); if (!ok) fails++; }

/* ---- a fake DIRECT screen that records RGB565 pixels ---- */
static uint16_t dp[H][W]; static long oob;
static void dff(qg_screen_t *s, int16_t x, int16_t y, int16_t w, int16_t h, qg_color_t c)
{ if (x < 0 || y < 0 || x + w > s->width || y + h > s->height) { oob++; return; }
  for (int j = y; j < y + h; j++) for (int i = x; i < x + w; i++) dp[j][i] = s->palette[c]; }
static void dwr(qg_screen_t *s, int16_t x, int16_t y, int16_t w, int16_t h, const uint16_t *px)
{ if (x < 0 || y < 0 || x + w > s->width || y + h > s->height) { oob++; return; }
  for (int j = 0; j < h; j++) for (int i = 0; i < w; i++) dp[y + j][x + i] = px[j * w + i]; }
static const qg_backend_t dback = { .name = "D", .fill_rect = dff, .write_rgb565 = dwr };

/* ---- a real BUF8 screen with guard bytes either side of its framebuffer ---- */
#define GUARD 4096
static uint8_t fbmem[GUARD + W * H + GUARD];
static uint8_t *const fb = fbmem + GUARD;
static int guards_intact(void)
{ for (int i = 0; i < GUARD; i++) if (fbmem[i] != 0xA5 || fbmem[GUARD + W * H + i] != 0xA5) return 0; return 1; }

/* The framebuffer's flush talks to the hardware; this test never flushes. */
void qg_hal_begin(qg_hal_device_t *d) { (void)d; }
void qg_hal_end(qg_hal_device_t *d) { (void)d; }
void qg_hal_stream_begin(qg_hal_device_t *d) { (void)d; }
static uint16_t flushed[W * H]; static uint32_t flushed_n;          /* what a flush sent */
void qg_hal_stream_pixels(qg_hal_device_t *d, const uint16_t *p, uint32_t n)
{ (void)d; for (uint32_t i = 0; i < n && flushed_n < W * H; i++) flushed[flushed_n++] = p[i]; }
void qg_hal_stream_end(qg_hal_device_t *d) { (void)d; }
void qg_driver_set_window(qg_hal_device_t *d, uint16_t a, uint16_t b, uint16_t c, uint16_t e) { (void)d; (void)a; (void)b; (void)c; (void)e; }

static qg_font_t f_body = QG_FONT_INIT(qg_font_sans_16, QG_DEFAULT, 1);

static void mk(qg_screen_t *s, int buffered)
{
    memset(s, 0, sizeof *s);
    s->width = W; s->height = H; s->ready = true;
    s->fg_color = QG_WHITE; s->bg_color = QG_BLACK; s->line_width = 1; s->line_style = 0xFFFF;
    s->text_bg = QG_TRANSPARENT; s->tab_width = 40; s->wrap = true; s->scroll = true;
    qg_palette_copy_standard(s->palette);
    qg_view_reset(s); { static qg_text_line_t qg_hist_pool[4][QG_TEXT_HISTORY_LINES]; static const void *qg_hist_owner[4]; int k_ = 0; while (k_ < 3 && qg_hist_owner[k_] && qg_hist_owner[k_] != (const void *)(s)) k_++; qg_hist_owner[k_] = (s); (s)->hist = qg_hist_pool[k_]; (s)->hist_cap = QG_TEXT_HISTORY_LINES; }
    if (buffered) { s->fb = fb; s->backend = &qg_backend_buf8; } else s->backend = &dback;
    qg_screen_set_font(s, 0, &f_body);
}
void qg_screen_set_line_width(qg_screen_t *s, uint8_t w) { s->line_width = w < 1 ? 1 : w; }
void qg_screen_set_colors(qg_screen_t *s, qg_color_t fg, qg_color_t bg) { if (fg <= 254) s->fg_color = fg; if (bg <= 254) s->bg_color = bg; }

static qg_image_t im_d20, im_land;

/* A scene touching every kind of drawing, offset by (ox, oy). Text wrapping
 * is off so the view's edge can't change where lines break.               */
static void scene(qg_screen_t *s, int ox, int oy)
{
    qg_screen_set_wrap(s, false);
    qg_circle(s, (int16_t)(60 + ox), (int16_t)(80 + oy), 50, QG_WHITE, QG_RED);
    qg_screen_set_line_width(s, 5);
    qg_line(s, (int16_t)(ox - 10), (int16_t)(20 + oy), (int16_t)(230 + ox), (int16_t)(300 + oy), QG_YELLOW);
    qg_arc(s, (int16_t)(150 + ox), (int16_t)(150 + oy), 70, 50, -30, 210, QG_LIGHTGREEN);
    qg_screen_set_line_width(s, 1);
    qg_ellipse(s, (int16_t)(120 + ox), (int16_t)(250 + oy), 90, 30, QG_CYAN, QG_MAGENTA);
    qg_box(s, (int16_t)(10 + ox), (int16_t)(180 + oy), (int16_t)(200 + ox), (int16_t)(215 + oy), QG_WHITE, QG_TRANSPARENT);
    qg_image_draw(s, &im_d20, (int16_t)(100 + ox), (int16_t)(120 + oy));
    qg_image_draw_scaled(s, &im_land, (int16_t)(20 + ox), (int16_t)(270 + oy), 200, 80);
    qg_print_at(s, (int16_t)(4 + ox), (int16_t)(40 + oy), "Clipped {c:YELLOW}text{c:} crosses the edge", QG_WHITE, NULL);
    qg_screen_set_text_bg(s, QG_DARKGRAY);
    qg_print_at(s, (int16_t)(70 + ox), (int16_t)(190 + oy), "Opaque", QG_WHITE, NULL);   /* fully inside the view */
    qg_screen_set_text_bg(s, QG_TRANSPARENT);
    qg_screen_set_wrap(s, true);
}

static uint16_t ref[H][W];
static int inside(int x, int y, int x0, int y0, int x1, int y1) { return x >= x0 && x <= x1 && y >= y0 && y <= y1; }

static void view_tests(void)
{
    qg_screen_t s;
    const int vx0 = 50, vy0 = 60, vx1 = 180, vy1 = 230;
    const uint16_t bg = 0; int bad;

    /* V1: clip only. */
    mk(&s, 0); qg_cls(&s, QG_BLUE); scene(&s, 0, 0); memcpy(ref, dp, sizeof ref);
    mk(&s, 0); qg_cls(&s, QG_BLUE); uint16_t blue = dp[0][0];
    qg_view(&s, vx0, vy0, vx1, vy1, false); scene(&s, 0, 0);
    bad = 0;
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++)
        bad += inside(x, y, vx0, vy0, vx1, vy1) ? (dp[y][x] != ref[y][x]) : (dp[y][x] != blue);
    check(bad == 0 && oob == 0, "VIEW clip-only: inside matches the unclipped scene, outside untouched");

    /* V2: origin moved. The scene at local (0,0) must equal the scene drawn
     * at (vx0, vy0) on the whole screen, inside the view.                  */
    mk(&s, 0); qg_cls(&s, QG_BLUE); scene(&s, vx0, vy0); memcpy(ref, dp, sizeof ref);
    mk(&s, 0); qg_cls(&s, QG_BLUE);
    qg_view(&s, vx0, vy0, vx1, vy1, true); scene(&s, 0, 0);
    bad = 0;
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++)
        bad += inside(x, y, vx0, vy0, vx1, vy1) ? (dp[y][x] != ref[y][x]) : (dp[y][x] != blue);
    check(bad == 0 && oob == 0, "VIEW with origin: local drawing == shifted drawing, clipped to the view");

    /* V3: CLS clears only the view. */
    qg_cls(&s, QG_RED); uint16_t red = s.palette[QG_RED];
    bad = 0;
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++)
        bad += inside(x, y, vx0, vy0, vx1, vy1) ? (dp[y][x] != red) : (dp[y][x] != blue);
    check(bad == 0, "VIEW: qg_cls clears just the view");

    /* V4: wrapping at the view's edge == qg_print_box at the same width. */
    const char *msg = "The quick brown fox jumps over the lazy dog, twice for luck.";
    mk(&s, 0); qg_cls(&s, QG_BLACK); qg_print_box(&s, vx0, vy0, vx1 - vx0 + 1, msg, QG_ALIGN_LEFT); memcpy(ref, dp, sizeof ref);
    mk(&s, 0); qg_cls(&s, QG_BLACK); qg_view(&s, vx0, vy0, vx1, vy1, true); qg_print_at(&s, 0, 0, msg, QG_DEFAULT, NULL);
    bad = 0;
    /* Inside the view the two must match. Outside, the view must have left
     * the screen black; qg_print_box may have drawn there, since a letter's
     * edge can hang a pixel outside its box (e.g. the tail of a 'j').     */
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++)
        bad += inside(x, y, vx0, vy0, vx1, vy1) ? (dp[y][x] != ref[y][x]) : (dp[y][x] != 0);
    check(bad == 0, "VIEW: text wraps at the view's edge exactly as qg_print_box does, and is clipped to it");

    /* V5: percentages measure the view when its origin is moved. */
    int16_t px = qg_pct_x(&s, 100), py = qg_pct_y(&s, 50);
    qg_view_reset(&s); { static qg_text_line_t qg_hist_pool[4][QG_TEXT_HISTORY_LINES]; static const void *qg_hist_owner[4]; int k_ = 0; while (k_ < 3 && qg_hist_owner[k_] && qg_hist_owner[k_] != (const void *)(&s)) k_++; qg_hist_owner[k_] = (&s); (&s)->hist = qg_hist_pool[k_]; (&s)->hist_cap = QG_TEXT_HISTORY_LINES; }
    check(px == vx1 - vx0 && py == (vy1 - vy0) / 2 && qg_pct_x(&s, 100) == W - 1,
          "VIEW: percentages measure the view (origin moved) or the screen");

    /* V6: PAINT and POINT stay inside the view (framebuffer screen). */
    memset(fbmem, 0xA5, sizeof fbmem);
    mk(&s, 1); qg_cls(&s, QG_BLACK); qg_view(&s, 20, 20, 99, 99, false);
    qg_err_t e = qg_paint(&s, 50, 50, QG_RED, QG_WHITE);
    bad = 0;
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++)
        bad += (fb[y * W + x] != (inside(x, y, 20, 20, 99, 99) ? QG_RED : QG_BLACK));
    check(e == QG_OK && bad == 0 && qg_point(&s, 10, 10) == QG_NONE && qg_point(&s, 30, 30) == QG_RED,
          "VIEW: PAINT fills only the view; POINT outside it gives QG_NONE");

    /* V7: cursor printing inside a view doesn't scroll, and stays inside. */
    mk(&s, 0); qg_cls(&s, QG_BLUE); qg_view(&s, vx0, vy0, vx1, vy1, true); qg_locate(&s, 0, 0);
    for (int i = 0; i < 30; i++) qg_println(&s, "Line of text");
    bad = 0;
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++)
        if (!inside(x, y, vx0, vy0, vx1, vy1)) bad += (dp[y][x] != blue);
    check(bad == 0 && oob == 0 && qg_csrlin(&s) > vy1 - vy0, "VIEW: printing past the view's bottom is cut off, no scrolling");
    check(guards_intact(), "VIEW: nothing written outside the framebuffer");
}

/* ------------------------------------------------------------------------ */
static void block_tests(void)
{
    qg_screen_t s;
    static uint8_t blk[QG_BLOCK_BYTES(32, 16)], before[W * H];
    int bad;

    memset(fbmem, 0xA5, sizeof fbmem);
    mk(&s, 1);
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) fb[y * W + x] = (uint8_t)((x * 7 + y * 13) % 255);

    /* P1: GET then PUT PSET copies the block exactly. */
    qg_err_t e = qg_get(&s, 10, 10, 41, 25, blk, sizeof blk);
    qg_put(&s, 100, 200, blk, QG_PUT_PSET);
    bad = 0;
    for (int j = 0; j < 16; j++) for (int i = 0; i < 32; i++) bad += fb[(200 + j) * W + 100 + i] != fb[(10 + j) * W + 10 + i];
    check(e == QG_OK && qg_block_width(blk) == 32 && qg_block_height(blk) == 16 && bad == 0, "GET/PUT PSET: copies exactly");

    /* P2: XOR twice restores the screen exactly. */
    memcpy(before, fb, sizeof before);
    qg_put(&s, 57, 91, blk, QG_PUT_XOR); int changed = memcmp(before, fb, sizeof before) != 0;
    qg_put(&s, 57, 91, blk, QG_PUT_XOR);
    check(changed && memcmp(before, fb, sizeof before) == 0, "PUT XOR: twice restores the screen exactly");

    /* P3: AND, OR and PRESET, byte by byte. */
    const qg_put_t modes[] = { QG_PUT_AND, QG_PUT_OR, QG_PUT_PRESET };
    const char *names[] = { "AND", "OR", "PRESET" };
    for (int m = 0; m < 3; m++) {
        memcpy(before, fb, sizeof before);
        qg_put(&s, 60, 60, blk, modes[m]);
        bad = 0;
        for (int j = 0; j < 16; j++) for (int i = 0; i < 32; i++) {
            uint8_t v = blk[4 + j * 32 + i], d = before[(60 + j) * W + 60 + i], got = fb[(60 + j) * W + 60 + i];
            uint8_t want = m == 0 ? (uint8_t)(d & v) : m == 1 ? (uint8_t)(d | v) : (uint8_t)(255 - v);
            bad += got != want;
        }
        char msg[64]; snprintf(msg, sizeof msg, "PUT %s: every byte as expected", names[m]); check(bad == 0, msg);
    }

    /* P4: TRANSPARENT skips 255. */
    for (int i = 0; i < 32 * 16; i += 3) blk[4 + i] = 255;
    memcpy(before, fb, sizeof before);
    qg_put(&s, 30, 150, blk, QG_PUT_TRANSPARENT);
    bad = 0;
    for (int j = 0; j < 16; j++) for (int i = 0; i < 32; i++) {
        uint8_t v = blk[4 + j * 32 + i];
        bad += fb[(150 + j) * W + 30 + i] != (v == 255 ? before[(150 + j) * W + 30 + i] : v);
    }
    check(bad == 0, "PUT TRANSPARENT: index 255 left the screen untouched, the rest copied");

    /* P5: clipped at every edge, nothing written outside the framebuffer. */
    qg_put(&s, -10, -5, blk, QG_PUT_PSET);
    qg_put(&s, W - 12, H - 7, blk, QG_PUT_XOR);
    check(guards_intact() && fb[0] == blk[4 + 5 * 32 + 10], "PUT: clipped at the screen edges, guards intact");

    /* P6: errors. */
    static uint8_t small[QG_BLOCK_BYTES(4, 4)];
    check(qg_get(&s, 0, 0, 7, 7, small, sizeof small) == QG_ERR_ARG, "GET: too small a buffer is refused");
    qg_view(&s, 20, 20, 99, 99, false);
    check(qg_get(&s, 10, 10, 30, 30, blk, sizeof blk) == QG_ERR_ARG, "GET: a rectangle leaving the view is refused");

    /* P7: PUT with a moved origin lands at the view's corner. */
    qg_view(&s, 20, 20, 99, 99, true);
    qg_err_t eg = qg_get(&s, 0, 0, 3, 3, small, sizeof small);
    qg_view_reset(&s); { static qg_text_line_t qg_hist_pool[4][QG_TEXT_HISTORY_LINES]; static const void *qg_hist_owner[4]; int k_ = 0; while (k_ < 3 && qg_hist_owner[k_] && qg_hist_owner[k_] != (const void *)(&s)) k_++; qg_hist_owner[k_] = (&s); (&s)->hist = qg_hist_pool[k_]; (&s)->hist_cap = QG_TEXT_HISTORY_LINES; }
    int ok = (eg == QG_OK); for (int j = 0; j < 4; j++) for (int i = 0; i < 4; i++) ok &= small[4 + j * 4 + i] == fb[(20 + j) * W + 20 + i];
    check(ok, "GET: coordinates are relative to a moved origin");

    /* P8: DIRECT screens: PSET/PRESET/TRANSPARENT through the palette; the rest refused. */
    mk(&s, 0); oob = 0; memset(dp, 0, sizeof dp);
    qg_put(&s, 5, 5, blk, QG_PUT_PSET);
    bad = 0; for (int j = 0; j < 16; j++) for (int i = 0; i < 32; i++) bad += dp[5 + j][5 + i] != s.palette[blk[4 + j * 32 + i]];
    memset(dp, 0, sizeof dp);
    qg_put(&s, 5, 5, blk, QG_PUT_TRANSPARENT);
    for (int j = 0; j < 16; j++) for (int i = 0; i < 32; i++) { uint8_t v = blk[4 + j * 32 + i]; bad += dp[5 + j][5 + i] != (v == 255 ? 0 : s.palette[v]); }
    qg_put(&s, 5, 50, blk, QG_PUT_PRESET);
    for (int j = 0; j < 16; j++) for (int i = 0; i < 32; i++) bad += dp[50 + j][5 + i] != s.palette[255 - blk[4 + j * 32 + i]];
    qg_put(&s, -20, H - 8, blk, QG_PUT_PSET);                     /* clipped */
    check(bad == 0 && oob == 0, "PUT on DIRECT: PSET, TRANSPARENT, PRESET correct; clipped safely");
    check(qg_put(&s, 0, 0, blk, QG_PUT_XOR) == QG_ERR_UNSUPPORTED && qg_put(&s, 0, 0, blk, QG_PUT_AND) == QG_ERR_UNSUPPORTED &&
          qg_get(&s, 0, 0, 7, 7, blk, sizeof blk) == QG_ERR_UNSUPPORTED, "DIRECT: GET and bitwise PUT report unsupported");
}

/* ------------------------------------------------------------------------ */
static int bit(uint16_t st, int k) { return (st >> (15 - (k & 15))) & 1; }
static int lit(int x, int y) { return dp[y][x] != 0; }

static void style_tests(void)
{
    qg_screen_t s; int bad;

    /* L1-L3: thin horizontal, vertical, diagonal: each pixel follows the pattern. */
    mk(&s, 0); memset(dp, 0, sizeof dp); qg_screen_set_line_style(&s, 0xF0F0);
    qg_line(&s, 0, 5, 79, 5, QG_WHITE); qg_line(&s, 100, 0, 100, 79, QG_WHITE);
    qg_screen_set_line_style(&s, 0xAAAA); qg_line(&s, 120, 0, 199, 79, QG_WHITE);
    bad = 0;
    for (int k = 0; k < 80; k++) bad += lit(k, 5) != bit(0xF0F0, k) || lit(100, k) != bit(0xF0F0, k) || lit(120 + k, k) != bit(0xAAAA, k);
    check(bad == 0, "LINE style: thin lines follow the pattern pixel by pixel (horizontal, vertical, diagonal)");

    /* L4: a thick dashed line: dashes are exactly the pattern's "on" columns, 4 rows deep. */
    mk(&s, 0); memset(dp, 0, sizeof dp); qg_screen_set_line_style(&s, 0xF0F0); qg_screen_set_line_width(&s, 4);
    qg_line(&s, 10, 100, 73, 100, QG_WHITE);
    bad = 0;
    for (int k = 0; k < 64; k++) { int rows = 0; for (int y = 90; y < 110; y++) rows += lit(10 + k, y); bad += rows != (bit(0xF0F0, k) ? 4 : 0); }
    check(bad == 0, "LINE style: thick dashes are exact, 4 pixels deep");

    /* L4b: thick DOTS (one pixel on, one off) must stay separate columns. */
    mk(&s, 0); memset(dp, 0, sizeof dp); qg_screen_set_line_style(&s, 0xAAAA); qg_screen_set_line_width(&s, 4);
    qg_line(&s, 10, 100, 73, 100, QG_WHITE);
    bad = 0;
    for (int k = 0; k < 64; k++) { int rows = 0; for (int y = 90; y < 110; y++) rows += lit(10 + k, y); bad += rows != (bit(0xAAAA, k) ? 4 : 0); }
    check(bad == 0, "LINE style: thick dots stay separate (one pixel long each)");

    /* L5: a styled box: the pattern runs once around it, carrying on round the corners. */
    mk(&s, 0); memset(dp, 0, sizeof dp); qg_screen_set_line_style(&s, 0xF0F0);
    const int l = 10, t = 10, r = 50, b = 35;
    qg_box(&s, l, t, r, b, QG_WHITE, QG_TRANSPARENT);
    bad = 0; int k = 0;
    for (int x = l; x <= r; x++, k++) bad += lit(x, t) != bit(0xF0F0, k);
    for (int y = t + 1; y <= b - 1; y++, k++) bad += lit(r, y) != bit(0xF0F0, k);
    for (int x = r; x >= l; x--, k++) bad += lit(x, b) != bit(0xF0F0, k);
    for (int y = b - 1; y >= t + 1; y--, k++) bad += lit(l, y) != bit(0xF0F0, k);
    check(bad == 0, "LINE style: box outline follows the pattern clockwise round all four sides");

    /* L6: a thick styled box: each "on" position of the top edge is lw deep. */
    mk(&s, 0); memset(dp, 0, sizeof dp); qg_screen_set_line_style(&s, 0xCCCC); qg_screen_set_line_width(&s, 3);
    qg_box(&s, 10, 10, 60, 40, QG_WHITE, QG_BLUE);
    bad = 0;
    for (int x = 10; x <= 60; x++) for (int y = 10; y < 13; y++) bad += lit(x, y) != bit(0xCCCC, x - 10);
    for (int y = 13; y <= 37; y++) for (int x = 13; x <= 57; x++) bad += dp[y][x] != s.palette[QG_BLUE];
    check(bad == 0, "LINE style: thick box sides follow the pattern, fill untouched");

    /* L7: 0 and 0xFFFF are both solid. */
    mk(&s, 0); memset(dp, 0, sizeof dp); qg_screen_set_line_style(&s, 0);
    qg_line(&s, 0, 0, 99, 0, QG_WHITE);
    bad = 0; for (int x = 0; x < 100; x++) bad += !lit(x, 0);
    check(bad == 0, "LINE style: 0 means solid");
}

/* ------------------------------------------------------------------------ */
static void misc_tests(void)
{
    qg_screen_t s;
    mk(&s, 0); qg_cls(&s, QG_BLUE); qg_screen_set_colors(&s, QG_WHITE, QG_GREEN);
    qg_pset(&s, 5, 5, QG_DEFAULT); qg_preset(&s, 6, 5, QG_DEFAULT); qg_preset(&s, 7, 5, QG_RED);
    check(dp[5][5] == s.palette[QG_WHITE] && dp[5][6] == s.palette[QG_GREEN] && dp[5][7] == s.palette[QG_RED],
          "PRESET: background colour by default, or the colour given");

    qg_locate(&s, 10, 20); qg_print(&s, "Hi");
    int16_t p = qg_pos(&s), c = qg_csrlin(&s);
    qg_view(&s, 30, 40, 200, 300, true); qg_locate(&s, 5, 6);
    check(p > 10 && c == 20 && qg_pos(&s) == 5 && qg_csrlin(&s) == 6,
          "CSRLIN/POS: cursor position, relative to the view's origin");

    uint8_t r, g, b, ok = 1;
    qg_palette_get(qg_palette_standard(), QG_RED, &r, &g, &b);   ok &= (r == 173 && g == 0 && b == 0);
    qg_palette_get(qg_palette_standard(), QG_WHITE, &r, &g, &b); ok &= (r == 255 && g == 255 && b == 255);
    qg_palette_set(&s, 200, 12, 34, 56); qg_palette_get(s.palette, 200, &r, &g, &b);
    ok &= (r == 8 && g == 32 && b == 57);                        /* RGB565 rounding, then widened */
    check(ok, "qg_palette_get: reads entries back as stored (RED = 173,0,0; WHITE = 255,255,255)");

    /* No text history given: printing past the bottom clears the screen and
     * carries on from the top, rather than scrolling.                      */
    mk(&s, 0); s.hist = NULL; s.hist_cap = 0; oob = 0;
    qg_cls(&s, QG_BLACK); qg_locate(&s, 0, 0);
    for (int i = 0; i < 40; i++) qg_println(&s, "A line of text");
    int16_t y_after = qg_csrlin(&s);
    int lit_rows = 0; for (int y = 0; y < H; y++) { int any = 0; for (int x = 0; x < W; x++) any |= dp[y][x] != 0; lit_rows += any; }
    check(oob == 0 && y_after < H && lit_rows > 0 && lit_rows < H,
          "No text history: printing past the bottom starts again at the top, safely");

    check(qg_palette_set(&s, 255, 1, 2, 3) == QG_OK && qg_palette_set(&s, 256, 0, 0, 0) == QG_ERR_ARG,
          "Palette entry 255 can be set; 256 is refused");
}

/* ------------------------------------------------------------------------ */
static uint8_t expect8(int v, int gain, int gamma)       /* the formula, independently */
{
    double o = 255.0 * gain / 100.0 * pow(v / 255.0, gamma / 100.0) + 0.5;
    return (uint8_t)(o > 255 ? 255 : o);
}

static qg_color_adjust_state_t adj_state;          /* the tables: our storage */

static void adjust_tests(void)
{
    qg_screen_t s;
    uint8_t r, g, b;
    int bad;

    /* A1: "no adjustment" sends exactly the palette. */
    mk(&s, 0);
    qg_color_adjust_t none = QG_COLOR_ADJUST_NONE;
    qg_screen_set_color_adjust(&s, &none, &adj_state);
    bad = 0; for (int i = 0; i < 256; i++) bad += adj_state.out[i] != s.palette[i];
    check(bad == 0, "COLOR ADJUST: all 100s sends exactly the palette");
    mk(&s, 0);
    check(s.adjust == NULL, "COLOR ADJUST: an unadjusted screen has no tables at all (nothing to pay)");

    /* A2: every entry follows the formula, per channel. */
    qg_color_adjust_t adj = { { 100, 90, 60 }, { 100, 150, 220 } };
    check(qg_screen_set_color_adjust(&s, &adj, &adj_state) == QG_OK, "COLOR ADJUST: accepted");
    bad = 0;
    for (int i = 0; i < 256; i++) {
        qg_palette_get(s.palette, (qg_color_t)i, &r, &g, &b);
        uint16_t want = QG_RGB565(expect8(r, 100, 100), expect8(g, 90, 150), expect8(b, 60, 220));
        bad += adj_state.out[i] != want;
    }
    qg_palette_get(s.palette, QG_WHITE, &r, &g, &b);
    check(bad == 0 && r == 255 && g == 255 && b == 255,
          "COLOR ADJUST: every entry adjusted per channel; qg_palette_get still gives the colour asked for");

    /* A3: later palette changes are adjusted too. */
    qg_palette_set(&s, 77, 200, 100, 50);
    qg_palette_get(s.palette, 77, &r, &g, &b);
    check(adj_state.out[77] == QG_RGB565(expect8(r, 100, 100), expect8(g, 90, 150), expect8(b, 60, 220)),
          "COLOR ADJUST: qg_palette_set entries are adjusted as they're set");

    /* A4: opaque text sends adjusted colours (DIRECT). */
    memset(dp, 0, sizeof dp);
    qg_screen_set_text_bg(&s, QG_BLUE);
    qg_print_at(&s, 10, 10, "Hi", QG_WHITE, NULL);
    qg_screen_set_text_bg(&s, QG_TRANSPARENT);
    check(dp[10][10] == adj_state.out[QG_BLUE], "COLOR ADJUST: opaque text background is sent adjusted");

    /* A5: images on DIRECT screens pass through the adjustment. */
    memset(dp, 0, sizeof dp);
    qg_image_draw(&s, &im_land, 0, 0);
    int ok = 1;
    {   /* recompute one pixel: find which image colour landed at (5, 5) */
        const uint8_t *pal = im_land.palette; int found = 0;
        for (int i = 0; i < im_land.palette_count; i++) {
            const uint8_t *e = &pal[i * 4];
            uint16_t plain = QG_RGB565(e[2], e[1], e[0]);
            uint16_t adjd  = QG_RGB565(expect8(e[2], 100, 100), expect8(e[1], 90, 150), expect8(e[0], 60, 220));
            if (dp[5][5] == adjd) found = 1;
            if (dp[5][5] == plain && plain != adjd) ok = 0;
        }
        ok = ok && found;
    }
    check(ok, "COLOR ADJUST: DIRECT image colours are sent adjusted");

    /* A6: a framebuffer flush sends adjusted colours. */
    memset(fbmem, 0xA5, sizeof fbmem);
    mk(&s, 1);
    qg_cls(&s, QG_LIGHTGRAY);
    qg_screen_set_color_adjust(&s, &adj, &adj_state);
    flushed_n = 0;
    s.backend->flush(&s);
    check(flushed_n == W * H && flushed[0] == adj_state.out[QG_LIGHTGRAY] && flushed[0] != s.palette[QG_LIGHTGRAY],
          "COLOR ADJUST: a framebuffer flush sends the whole screen, adjusted");

    /* A7: off again, and bad values refused. */
    qg_screen_set_color_adjust(&s, NULL, NULL);
    flushed_n = 0; s.backend->flush(&s);
    qg_color_adjust_t too_bright = { { 101, 100, 100 }, { 100, 100, 100 } };
    qg_color_adjust_t too_low    = { { 100, 100, 100 }, { 100, 40, 100 } };
    check(flushed[0] == s.palette[QG_LIGHTGRAY] &&
          qg_screen_set_color_adjust(&s, &too_bright, &adj_state) == QG_ERR_ARG &&
          qg_screen_set_color_adjust(&s, &too_low, &adj_state) == QG_ERR_ARG &&
          qg_screen_set_color_adjust(&s, &adj, NULL) == QG_ERR_ARG,
          "COLOR ADJUST: NULL switches it off; out-of-range values, or no storage, refused");
}

int main(void)
{
    qg_image_open(&im_d20, img_d20, img_d20_size, QG_IMAGE_TRANSPARENT);
    qg_image_open(&im_land, img_landscape, img_landscape_size, 0);
    view_tests();
    block_tests();
    style_tests();
    misc_tests();
    adjust_tests();
    printf(fails ? "\n%d FAILED\n" : "\nnew commands: all pass\n", fails);
    return fails != 0;
}
