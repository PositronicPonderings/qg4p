/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/*
 * Tests for the photosensitivity-safer flash (qg_flash.c), with simulated
 * time: no Pico, no backlight, no waiting. The fake qg_screen_set_brightness
 * below writes down every brightness the flash asks for, and at what time,
 * and the checks hold that log against the promises in qg_flash.h:
 *
 *   a) the rate cap: in every one-second window, at most 4 opposing changes
 *      (2 flashes), even when asked for 10 flashes a second, with loops fast,
 *      slow and erratic, and with flashes restarted back to back;
 *   b) gradual: no single update moves brightness more than QG_FLASH_MAX_STEP;
 *   c) saturated red refused (QG_ERR_ARG), checked against an independent
 *      R / (R + G + B) >= 0.8 for every one of the 65,536 RGB565 colours;
 *   d) it ends by itself after at most QG_FLASH_MAX_COUNT flashes, with the
 *      screen restored exactly (brightness, pixels, palette, everything);
 *   e) cancelling, smoothly or at once, restores the screen exactly.
 *
 * The counting is deliberately stricter than WCAG's: a change counts as
 * "in" a window if any part of it touches the window, and each change is
 * taken to begin at the update BEFORE its first step (it might have begun
 * any time after that, and we assume the worst).
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "qg4p.h"
#include "qg_flash.h"
#include "qg_internal.h"

static int fails = 0;
static void check(int ok, const char *what) { printf("%s  %s\n", ok ? "PASS" : "FAIL", what); if (!ok) fails++; }

/* ---- the fake hardware: a brightness recorder ---- */
static uint32_t sim_now;                         /* the simulated clock, ms */
static long brightness_writes;
void qg_screen_set_brightness(qg_screen_t *s, uint8_t p) { s->brightness = p > 100 ? 100 : p; brightness_writes++; }

/* ---- a backend that counts any pixel work (the flash must do none) ---- */
static long pixel_calls;
static void bf(qg_screen_t *s, int16_t x, int16_t y, int16_t w, int16_t h, qg_color_t c) { (void)s; (void)x; (void)y; (void)w; (void)h; (void)c; pixel_calls++; }
static void bw(qg_screen_t *s, int16_t x, int16_t y, int16_t w, int16_t h, const uint16_t *p) { (void)s; (void)x; (void)y; (void)w; (void)h; (void)p; pixel_calls++; }
static void bflush(qg_screen_t *s) { (void)s; pixel_calls++; }
static const qg_backend_t counting = { .name = "COUNT", .fill_rect = bf, .write_rgb565 = bw, .flush = bflush };

#define W 240
#define H 320
static uint8_t fbuf[W * H];

static void mk(qg_screen_t *s, int buffered, uint8_t brightness)
{
    memset(s, 0, sizeof *s);
    s->width = W; s->height = H; s->ready = true;
    s->cfg.bl_pin = 16; s->cfg.bl_active_high = true;
    s->bl_on = true; s->brightness = brightness;
    s->fg_color = QG_WHITE; s->bg_color = QG_BLACK;
    s->backend = &counting;
    qg_palette_copy_standard(s->palette);
    if (buffered) {                       /* a framebuffer full of something */
        for (int i = 0; i < W * H; i++) fbuf[i] = (uint8_t)(i * 7 + 3);
        s->fb = fbuf;
    }
}

/* ---- the log: one sample per update, (time, brightness after it) ---- */
#define MAXS 200000
static uint32_t lt[MAXS]; static uint8_t lb[MAXS]; static int ln;
static void log_reset(const qg_screen_t *s) { ln = 0; lt[ln] = sim_now; lb[ln++] = s->brightness; }
static void log_sample(const qg_screen_t *s) { if (ln < MAXS) { lt[ln] = sim_now; lb[ln++] = s->brightness; } }

/* Loop speeds. Positive: a fixed step, ms. Negative: random steps 1..-n. */
static uint32_t rng = 12345;
static uint32_t next_dt(int pattern)
{
    if (pattern > 0) return (uint32_t)pattern;
    rng = rng * 1103515245u + 12345u;
    return 1u + (rng >> 16) % (uint32_t)(-pattern);
}

/* Run the flash until it ends (or `limit` ms pass), logging every update. */
static uint32_t run(qg_flash_t *f, qg_screen_t *s, int pattern, uint32_t limit)
{
    uint32_t t0 = sim_now;
    while (sim_now - t0 < limit) {
        sim_now += next_dt(pattern);
        bool on = qg_flash_update(f, sim_now);
        log_sample(s);
        if (!on) break;
    }
    return sim_now - t0;
}

/* ---- analysis of the log ---- */
typedef struct { uint32_t start, end; int sign; } change_t;
static change_t ch[MAXS]; static int nch;

/* Split the log into changes: maximal runs of steps in one direction
 * (steps of zero don't break a run). A change begins at the sample BEFORE
 * its first step, and ends at its last step.                              */
static void find_changes(void)
{
    nch = 0;
    for (int i = 1; i < ln; i++) {
        int d = (int)lb[i] - (int)lb[i - 1];
        if (d == 0) continue;
        int sg = d > 0 ? 1 : -1;
        if (nch == 0 || ch[nch - 1].sign != sg) { ch[nch].start = lt[i - 1]; ch[nch].sign = sg; nch++; }
        ch[nch - 1].end = lt[i];
    }
}

/* The most changes that touch any one-second window. Changes j..j+m all
 * touch some window [w, w + 1000) exactly when change j+m starts less than
 * 1000 ms after change j ends.                                            */
static int max_changes_per_second(void)
{
    int worst = nch ? 1 : 0;
    for (int j = 0; j < nch; j++) {
        int m = 0;
        while (j + m + 1 < nch && (int32_t)(ch[j + m + 1].start - ch[j].end) < 1000) m++;
        if (m + 1 > worst) worst = m + 1;
    }
    return worst;
}

static int max_step(void)
{
    int worst = 0;
    for (int i = 1; i < ln; i++) { int d = abs((int)lb[i] - (int)lb[i - 1]); if (d > worst) worst = d; }
    return worst;
}

/* ========================================================================= */

static void test_rate_and_smoothness(void)
{
    /* Every loop speed we could think of, from a frantic 1 ms to a sluggish
     * 120 ms, steady and erratic. Every request is "ten a second, five of
     * them, all the way to black": the worst a caller can ask for.        */
    const int patterns[] = { 1, 5, 10, 16, 20, 33, 72, -40, -120 };
    const qg_flash_config_t greedy = { .period_ms = 100, .count = 50, .depth = 100 };
    int worst_rate = 0, worst_step = 0, all_ended = 1, all_restored = 1, min_dip = 1000;
    for (unsigned k = 0; k < sizeof patterns / sizeof patterns[0]; k++) {
        qg_screen_t s; qg_flash_t f; memset(&f, 0, sizeof f);
        mk(&s, 0, 100); sim_now = 1000; log_reset(&s);
        if (qg_flash_start(&f, &s, &greedy, sim_now) != QG_OK) all_ended = 0;
        run(&f, &s, patterns[k], 120000);
        all_ended &= !qg_flash_active(&f);
        all_restored &= s.brightness == 100;
        find_changes();
        int r = max_changes_per_second(), st = max_step();
        if (r > worst_rate) worst_rate = r;
        if (st > worst_step) worst_step = st;
        /* Gradual in time, too: no complete change is a snap. Each ramp is
         * at least 300 ms of flash time, so at least ~270 ms in real time
         * between its first and last step even at the fastest loop.        */
        for (int j = 0; j < nch; j++) { int d = (int)(ch[j].end - ch[j].start); if (d < min_dip) min_dip = d; }
        printf("      loop %4d ms: %d changes, at most %d in any second, biggest step %d\n",
               patterns[k], nch, r, st);
    }
    char msg[200];
    snprintf(msg, sizeof msg, "a) 10 flashes/s requested: at most %d opposing changes in any 1 s window (cap: 4 = 2 flashes)", worst_rate);
    check(worst_rate <= 4, msg);
    snprintf(msg, sizeof msg, "b) gradual: biggest single step %d points (limit QG_FLASH_MAX_STEP = %u)", worst_step, QG_FLASH_MAX_STEP);
    check(worst_step <= (int)QG_FLASH_MAX_STEP, msg);
    snprintf(msg, sizeof msg, "b) gradual: every change lasts at least 250 ms (shortest: %d ms)", min_dip);
    check(min_dip >= 250, msg);
    check(all_ended && all_restored, "d) greedy requests still end by themselves, brightness restored to 100");

    /* Back to back: the caller restarts the same flash the moment it ends,
     * and sometimes cancels it half-way (smoothly, or at once) and restarts
     * that too. For a whole
     * simulated minute. The cap must hold across the joins.               */
    worst_rate = 0; worst_step = 0;
    const int bb_patterns[] = { 1, 10, 20, -40 };
    for (int abrupt = 0; abrupt <= 1; abrupt++)
    for (unsigned k = 0; k < sizeof bb_patterns / sizeof bb_patterns[0]; k++) {
        qg_screen_t s; qg_flash_t f; memset(&f, 0, sizeof f);
        mk(&s, 0, 90); sim_now = 5000; log_reset(&s);
        uint32_t t0 = sim_now; int n = 0;
        qg_flash_start(&f, &s, &greedy, sim_now);
        while (sim_now - t0 < 60000) {
            sim_now += next_dt(bb_patterns[k]);
            bool on = qg_flash_update(&f, sim_now);
            log_sample(&s);
            if (!on) { qg_flash_start(&f, &s, &greedy, sim_now); n++; }
            else if (((sim_now / 7) % 97) == 0) qg_flash_cancel(&f, sim_now, false);           /* now and then */
            else if (abrupt && ((sim_now / 11) % 89) == 0) qg_flash_cancel(&f, sim_now, true); /* and abruptly */
            log_sample(&s);            /* an at-once cancel changes it right here */
        }
        find_changes();
        int r = max_changes_per_second(), st = max_step();
        if (r > worst_rate) worst_rate = r;
        if (!abrupt && st > worst_step) worst_step = st;   /* at once = one jump, by design */
        printf("      back-to-back, %s cancels, loop %4d ms: %d restarts, %d changes, at most %d in any second\n",
               abrupt ? "smooth+abrupt" : "smooth", bb_patterns[k], n, nch, r);
    }
    snprintf(msg, sizeof msg, "a) restarted and cancelled back to back for a minute: at most %d changes in any 1 s window", worst_rate);
    check(worst_rate <= 4, msg);
    snprintf(msg, sizeof msg, "b) ... and with smooth cancels, biggest step %d (at-once cancels jump back by design)", worst_step);
    check(worst_step <= (int)QG_FLASH_MAX_STEP, msg);

    /* And at the defaults: a calm one a second, half depth.               */
    {
        qg_screen_t s; qg_flash_t f; memset(&f, 0, sizeof f);
        mk(&s, 0, 100); sim_now = 0; log_reset(&s);
        qg_flash_start(&f, &s, NULL, sim_now);
        run(&f, &s, 10, 60000);
        find_changes();
        int lowest = 100; for (int i = 0; i < ln; i++) if (lb[i] < lowest) lowest = lb[i];
        snprintf(msg, sizeof msg, "defaults: 3 flashes (%d changes), dimmed only to %d %%, at most %d changes per second, steps <= %d",
                 nch, lowest, max_changes_per_second(), max_step());
        check(nch == 6 && lowest == 50 && max_changes_per_second() <= 4 && max_step() <= 5, msg);
    }
}

static void test_red(void)
{
    qg_screen_t s; qg_flash_t f; memset(&f, 0, sizeof f);
    int bad = 0;

    /* Every RGB565 colour, against an independent floating-point check of
     * WCAG's R / (R + G + B) >= 0.8 on the 8-bit values the screen sends. */
    mk(&s, 0, 100);
    for (uint32_t c = 0; c < 65536; c++) {
        s.palette[16] = (uint16_t)c;
        uint8_t r, g, b; qg_palette_get(s.palette, 16, &r, &g, &b);
        double sum = (double)r + g + b;
        bool expect = sum > 0 && r / sum >= 0.8 - 1e-12;
        bad += qg_flash_is_saturated_red(&s, 16) != expect;
    }
    check(bad == 0, "c) saturated-red test agrees with R/(R+G+B) >= 0.8 for all 65,536 RGB565 colours");

    mk(&s, 0, 100);
    check(qg_flash_is_saturated_red(&s, QG_RED) && !qg_flash_is_saturated_red(&s, QG_LIGHTRED) &&
          !qg_flash_is_saturated_red(&s, QG_MAGENTA) && !qg_flash_is_saturated_red(&s, QG_YELLOW) &&
          !qg_flash_is_saturated_red(&s, QG_BLACK) && !qg_flash_is_saturated_red(&s, QG_DEFAULT) &&
          !qg_flash_is_saturated_red(&s, QG_NONE),
          "c) QG_RED is saturated red; LIGHTRED (0.6), MAGENTA, YELLOW, BLACK and the special values are not");

    /* Refused three ways: background, foreground, the highlighted colour.
     * Refused means refused: nothing changes, nothing starts.             */
    int ok = 1;
    mk(&s, 0, 80); s.bg_color = QG_RED; brightness_writes = 0;
    ok &= qg_flash_start(&f, &s, NULL, 0) == QG_ERR_ARG && !qg_flash_active(&f);
    mk(&s, 0, 80); s.fg_color = QG_RED;
    ok &= qg_flash_start(&f, &s, NULL, 0) == QG_ERR_ARG && !qg_flash_active(&f);
    mk(&s, 0, 80);
    qg_flash_config_t red = { .color = QG_RED };
    ok &= qg_flash_start(&f, &s, &red, 0) == QG_ERR_ARG && !qg_flash_active(&f);
    for (int i = 0; i < 100; i++) qg_flash_update(&f, (uint32_t)i * 10);
    ok &= s.brightness == 80 && brightness_writes == 0;
    check(ok, "c) a saturated red background, foreground or highlight colour is refused with QG_ERR_ARG; nothing flashes");

    mk(&s, 0, 80); s.fg_color = QG_LIGHTRED;
    qg_flash_config_t yellow = { .color = QG_YELLOW };
    check(qg_flash_start(&f, &s, &yellow, 0) == QG_OK && qg_flash_active(&f),
          "c) non-red colours (LIGHTRED text, a YELLOW highlight) are accepted");
}

static void test_ending_and_restoring(void)
{
    for (int buffered = 0; buffered <= 1; buffered++) {
        qg_screen_t s, before; qg_flash_t f; memset(&f, 0, sizeof f);
        static uint8_t fb_before[W * H];
        mk(&s, buffered, 73);                    /* an awkward brightness   */
        s.dirty = false;
        memcpy(&before, &s, sizeof s);
        if (buffered) memcpy(fb_before, fbuf, sizeof fb_before);
        pixel_calls = 0;

        sim_now = 77; log_reset(&s);
        qg_flash_config_t many = { .period_ms = 900, .count = 200, .depth = 60 };
        qg_flash_start(&f, &s, &many, sim_now);
        uint32_t took = run(&f, &s, 10, 600000);
        find_changes();
        int dips = 0; for (int j = 0; j < nch; j++) dips += ch[j].sign < 0;

        /* 5 flashes of 900 ms, minus the last rest: 4 x 900 + 2 x 337 = 4274. */
        int same = memcmp(&s, &before, sizeof s) == 0 && pixel_calls == 0 &&
                   (!buffered || memcmp(fbuf, fb_before, sizeof fb_before) == 0);
        char msg[200];
        snprintf(msg, sizeof msg, "d) %s: 200 flashes asked, %d given, over by itself in %u ms; screen restored exactly, no pixel touched",
                 buffered ? "framebuffer screen" : "DIRECT screen", dips, (unsigned)took);
        check(dips == (int)QG_FLASH_MAX_COUNT && !qg_flash_active(&f) && took <= 4280 && same, msg);
    }
}

static void test_cancel(void)
{
    char msg[200];
    /* Smooth cancel, at every point of a flash: part-way down, at the
     * bottom, part-way up, resting, and before it starts.                 */
    const uint32_t when[] = { 5, 100, 239, 250, 400, 470, 600, 1000, 1500 };
    int ok = 1, worst_step = 0; uint32_t longest = 0;
    for (unsigned k = 0; k < sizeof when / sizeof when[0]; k++) {
        qg_screen_t s, before; qg_flash_t f; memset(&f, 0, sizeof f);
        mk(&s, k & 1, 64); memcpy(&before, &s, sizeof s); pixel_calls = 0;
        sim_now = 0; log_reset(&s);
        qg_flash_config_t cfg = { .period_ms = 640, .depth = 100 };
        qg_flash_start(&f, &s, &cfg, sim_now);
        run(&f, &s, 5, when[k]);
        qg_flash_cancel(&f, sim_now, false);
        uint32_t took = run(&f, &s, 5, 10000);
        if (took > longest) longest = took;
        if (max_step() > worst_step) worst_step = max_step();
        ok &= !qg_flash_active(&f) && memcmp(&s, &before, sizeof s) == 0 && pixel_calls == 0;
    }
    snprintf(msg, sizeof msg, "e) smooth cancel anywhere in a flash: restored exactly, within %u ms, steps <= %d",
             (unsigned)longest, worst_step);
    check(ok && longest <= 310 && worst_step <= (int)QG_FLASH_MAX_STEP, msg);

    /* At-once cancel, at the very bottom of a full-depth dip.             */
    {
        qg_screen_t s; qg_flash_t f; memset(&f, 0, sizeof f);
        mk(&s, 0, 64); sim_now = 0;
        qg_flash_config_t cfg = { .period_ms = 640, .depth = 100 };
        qg_flash_start(&f, &s, &cfg, sim_now);
        log_reset(&s); run(&f, &s, 5, 300);
        uint8_t low = s.brightness;
        qg_flash_cancel(&f, sim_now, true);
        snprintf(msg, sizeof msg, "e) cancel at once: from %u %% straight back to 64 %%, flash stopped", low);
        check(low < 10 && s.brightness == 64 && !qg_flash_active(&f) && !qg_flash_update(&f, sim_now + 5), msg);
    }

    /* Someone else changes the brightness mid-flash: the flash steps aside
     * and leaves their setting alone.                                      */
    {
        qg_screen_t s; qg_flash_t f; memset(&f, 0, sizeof f);
        mk(&s, 0, 100); sim_now = 0;
        qg_flash_start(&f, &s, NULL, sim_now);
        run(&f, &s, 10, 200);
        qg_screen_set_brightness(&s, 30);
        bool still = qg_flash_update(&f, sim_now + 10);
        check(!still && s.brightness == 30, "e) brightness changed by the program mid-flash: the flash stops and keeps its hands off");
    }
}

static void test_odds_and_ends(void)
{
    qg_screen_t s; qg_flash_t f; memset(&f, 0, sizeof f);

    mk(&s, 0, 100); s.cfg.bl_pin = QG_PIN_NONE;
    check(qg_flash_start(&f, &s, NULL, 0) == QG_ERR_UNSUPPORTED, "no backlight pin: QG_ERR_UNSUPPORTED");

    mk(&s, 0, 100); s.bl_on = false; brightness_writes = 0;
    check(qg_flash_start(&f, &s, NULL, 0) == QG_OK && !qg_flash_active(&f) && brightness_writes == 0,
          "backlight off: nothing to flash, QG_OK, nothing touched");

    check(qg_flash_start(NULL, &s, NULL, 0) == QG_ERR_ARG && qg_flash_start(&f, NULL, NULL, 0) == QG_ERR_ARG &&
          !qg_flash_update(NULL, 0) && !qg_flash_update(&f, 0) && (qg_flash_cancel(NULL, 0, true), 1),
          "NULLs refused; update and cancel on an idle flash are harmless");

    /* Starting again while flashing changes nothing (no stacking).        */
    mk(&s, 0, 100); memset(&f, 0, sizeof f); sim_now = 0;
    qg_flash_start(&f, &s, NULL, sim_now);
    run(&f, &s, 10, 300);
    uint32_t t_before = f.t;
    check(qg_flash_start(&f, &s, NULL, sim_now) == QG_OK && f.t == t_before && qg_flash_active(&f),
          "start while already flashing: carries on, doesn't restart or stack");

    /* The clock wrapping round (49.7 days in) changes nothing.            */
    mk(&s, 0, 100); memset(&f, 0, sizeof f);
    sim_now = 0xFFFFFF00u; log_reset(&s);
    qg_flash_start(&f, &s, NULL, sim_now);
    uint32_t took = run(&f, &s, 10, 60000);
    find_changes();
    check(nch == 6 && s.brightness == 100 && took < 2800, "the millisecond clock wrapping round mid-flash doesn't matter");
}

int main(void)
{
    test_rate_and_smoothness();
    test_red();
    test_ending_and_restoring();
    test_cancel();
    test_odds_and_ends();
    printf("%s\n", fails ? "SOME FLASH TESTS FAILED" : "all flash tests passed");
    return fails != 0;
}
