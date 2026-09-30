/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    qg_palette.c
 * @brief   Builds the standard palette and provides colour names.
 *
 * LAYER:   Public API (colour)
 * DEPENDS: qg_palette.h
 *
 * WHY BUILD IT AT RUNTIME INSTEAD OF A 256-LINE TABLE?
 * 240 of the 256 entries follow simple formulas (the cube and the grey ramp).
 * A short loop is easier to read, check and modify than 240 hex constants.
 * It runs once, in well under a millisecond, and costs 512 bytes of RAM.
 */
#include <stddef.h>
#include <stdint.h>
#include "qg_palette.h"
#include "qg_screen.h"
#include "qg_internal.h"
#include <math.h>

/* The classic 16 colours, as the VGA card displayed them. Note BROWN: it is
 * not "dark yellow" (0xAA,0xAA,0x00). The original hardware halved the green
 * on purpose, and QuickBasic programs expect that look.                     */
static const uint8_t qb16_rgb[16][3] = {
    { 0x00, 0x00, 0x00 },  /*  0 BLACK        */
    { 0x00, 0x00, 0xAA },  /*  1 BLUE         */
    { 0x00, 0xAA, 0x00 },  /*  2 GREEN        */
    { 0x00, 0xAA, 0xAA },  /*  3 CYAN         */
    { 0xAA, 0x00, 0x00 },  /*  4 RED          */
    { 0xAA, 0x00, 0xAA },  /*  5 MAGENTA      */
    { 0xAA, 0x55, 0x00 },  /*  6 BROWN        */
    { 0xAA, 0xAA, 0xAA },  /*  7 LIGHTGRAY    */
    { 0x55, 0x55, 0x55 },  /*  8 DARKGRAY     */
    { 0x55, 0x55, 0xFF },  /*  9 LIGHTBLUE    */
    { 0x55, 0xFF, 0x55 },  /* 10 LIGHTGREEN   */
    { 0x55, 0xFF, 0xFF },  /* 11 LIGHTCYAN    */
    { 0xFF, 0x55, 0x55 },  /* 12 LIGHTRED     */
    { 0xFF, 0x55, 0xFF },  /* 13 LIGHTMAGENTA */
    { 0xFF, 0xFF, 0x55 },  /* 14 YELLOW       */
    { 0xFF, 0xFF, 0xFF },  /* 15 WHITE        */
};

static const char *const qb16_names[16] = {
    "BLACK", "BLUE", "GREEN", "CYAN", "RED", "MAGENTA", "BROWN", "LIGHTGRAY",
    "DARKGRAY", "LIGHTBLUE", "LIGHTGREEN", "LIGHTCYAN", "LIGHTRED",
    "LIGHTMAGENTA", "YELLOW", "WHITE",
};

static uint16_t s_standard[256];
static bool     s_built = false;

static void build_standard(void)
{
    uint16_t i = 0;

    /* 0..15: the named colours. */
    for (; i < 16; i++) {
        s_standard[i] = QG_RGB565(qb16_rgb[i][0], qb16_rgb[i][1], qb16_rgb[i][2]);
    }

    /* 16..231: a 6x6x6 cube. Each channel steps 0, 51, 102, 153, 204, 255,
     * the old "web-safe" colours. Blue changes fastest, then green, then red,
     * which gives the index formula 16 + 36*r + 6*g + b.                    */
    for (uint8_t r = 0; r < 6; r++) {
        for (uint8_t g = 0; g < 6; g++) {
            for (uint8_t b = 0; b < 6; b++) {
                s_standard[i++] = QG_RGB565(r * 51, g * 51, b * 51);
            }
        }
    }

    /* 232..254: 23 greys. We skip pure black and pure white because index 0
     * and 15 already have them, so the ramp runs (n+1)*255/24 = 10..244.   */
    for (uint8_t n = 0; n < 23; n++) {
        uint8_t v = (uint8_t)(((n + 1) * 255) / 24);
        s_standard[i++] = QG_RGB565(v, v, v);
    }

    /* 255: transparent. Never drawn on purpose, so make it loud if it is. */
    s_standard[255] = QG_RGB565(0xFF, 0x00, 0xFF);

    s_built = true;
}

const uint16_t *qg_palette_standard(void)
{
    if (!s_built) {
        build_standard();
    }
    return s_standard;
}

void qg_palette_copy_standard(uint16_t dst[256])
{
    const uint16_t *src = qg_palette_standard();
    for (int i = 0; i < 256; i++) {
        dst[i] = src[i];
    }
}

const char *qg_color_name(qg_color_t c)
{
    return (c < 16) ? qb16_names[c] : NULL;
}

/* ========================================================================== */
/*  RGB lookup and palette changes                                            */
/* ========================================================================== */

/*
 * Unpack RGB565 back to 8 bits per channel.
 * Shifting left refills the top bits; copying the top bits into the empty low
 * bits spreads the values evenly, so 5-bit 31 becomes 255, not 248.
 */
static inline void rgb565_unpack(uint16_t c, int32_t *r, int32_t *g, int32_t *b)
{
    uint32_t r5 = (c >> 11) & 0x1F;
    uint32_t g6 = (c >> 5)  & 0x3F;
    uint32_t b5 =  c        & 0x1F;
    *r = (int32_t)((r5 << 3) | (r5 >> 2));
    *g = (int32_t)((g6 << 2) | (g6 >> 4));
    *b = (int32_t)((b5 << 3) | (b5 >> 2));
}

qg_color_t qg_color_from_rgb(uint8_t r, uint8_t g, uint8_t b, const uint16_t *palette)
{
    if (palette == NULL) {
        palette = qg_palette_standard();
    }

    /*
     * NEAREST COLOUR, WEIGHTED FOR THE EYE
     * We measure "distance" between colours as a weighted sum of squared
     * channel differences. The eye is most sensitive to green and least to
     * blue, so a small green error counts more than the same error in blue.
     * The weights 2 / 4 / 3 are a cheap, widely used approximation.
     */
    qg_color_t best      = 0;
    int32_t     best_dist = INT32_MAX;

    for (int i = 0; i < 255; i++) {          /* 255 = transparent: skip */
        int32_t pr, pg, pb;
        rgb565_unpack(palette[i], &pr, &pg, &pb);

        int32_t dr = pr - r, dg = pg - g, db = pb - b;
        int32_t dist = 2 * dr * dr + 4 * dg * dg + 3 * db * db;

        if (dist < best_dist) {
            best_dist = dist;
            best      = (qg_color_t)i;
            if (dist == 0) {
                break;                       /* exact match: can't do better */
            }
        }
    }
    return best;
}

qg_err_t qg_palette_set(qg_screen_t *scr, qg_color_t index,
                          uint8_t r, uint8_t g, uint8_t b)
{
    if (scr == NULL || index > 255) {
        return QG_ERR_ARG;
    }
    scr->palette[index] = QG_RGB565(r, g, b);
    qg_int_palette_sync(scr, index, 1);   /* the as-sent copy, if adjusting */
    scr->palette_gen++;          /* image colour-matching caches are now stale */
    qg_int_dirty_all(scr);      /* BUF8: every pixel may have changed colour  */
    return QG_OK;
}

void qg_palette_reset(qg_screen_t *scr)
{
    if (scr != NULL) {
        qg_palette_copy_standard(scr->palette);
        qg_int_palette_sync(scr, 0, 256);
        scr->palette_gen++;
        qg_int_dirty_all(scr);
    }
}

int qg_color_from_name(const char *name, size_t len)
{
    for (int i = 0; i < 16; i++) {
        const char *n = qb16_names[i];
        size_t k = 0;
        /* Compare letter by letter, folding lower case to upper case. */
        while (k < len && n[k] != '\0') {
            char c = name[k];
            if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
            if (c != n[k]) break;
            k++;
        }
        if (k == len && n[k] == '\0') {
            return i;
        }
    }
    return -1;
}

void qg_palette_get(const uint16_t *palette, qg_color_t index,
                    uint8_t *r, uint8_t *g, uint8_t *b)
{
    if (palette == NULL || index > 255) { *r = *g = *b = 0; return; }
    uint16_t c = palette[index];
    uint32_t r5 = (c >> 11) & 0x1F, g6 = (c >> 5) & 0x3F, b5 = c & 0x1F;
    /* Widen to 8 bits by repeating the top bits in the gap, so that full
     * brightness (31 or 63) becomes exactly 255 rather than 248 or 252.  */
    *r = (uint8_t)((r5 << 3) | (r5 >> 2));
    *g = (uint8_t)((g6 << 2) | (g6 >> 4));
    *b = (uint8_t)((b5 << 3) | (b5 >> 2));
}

/* ========================================================================== */
/*  Colour adjustment                                                         */
/* ========================================================================== */

void qg_int_palette_sync(qg_screen_t *s, int first, int count)
{
    if (s->adjust == NULL) return;          /* not adjusting: nothing to keep up */
    for (int i = first; i < first + count && i < 256; i++) {
        uint8_t r, g, b;
        qg_palette_get(s->palette, (qg_color_t)i, &r, &g, &b);
        s->adjust->out[i] = qg_int_out_rgb(s, r, g, b);
    }
}

qg_err_t qg_screen_set_color_adjust(qg_screen_t *scr, const qg_color_adjust_t *adj,
                                    qg_color_adjust_state_t *state)
{
    if (scr == NULL) return QG_ERR_ARG;
    if (adj == NULL) {
        scr->adjust = NULL;                 /* your storage is free to reuse    */
        qg_int_dirty_all(scr);              /* BUF8: resend in the plain colours */
        return QG_OK;
    }
    if (state == NULL) return QG_ERR_ARG;
    for (int c = 0; c < 3; c++) {
        if (adj->gain[c] > 100 || adj->gamma[c] < 50 || adj->gamma[c] > 300) return QG_ERR_ARG;
    }

    /*
     * One lookup table per channel: out = full * (in / 255) ^ gamma, where
     * full = 255 * gain / 100. It's built once here (768 powf calls, a
     * millisecond or two), so using it costs nothing per pixel.
     */
    state->settings = *adj;
    for (int c = 0; c < 3; c++) {
        float full = 255.0f * (float)adj->gain[c] / 100.0f;
        float gam  = (float)adj->gamma[c] / 100.0f;
        for (int v = 0; v < 256; v++) {
            float out = full * powf((float)v / 255.0f, gam) + 0.5f;
            state->lut[c][v] = (uint8_t)(out > 255.0f ? 255.0f : out);
        }
    }
    scr->adjust = state;
    qg_int_palette_sync(scr, 0, 256);
    scr->palette_gen++;
    qg_int_dirty_all(scr);                  /* BUF8: the next flush resends all */
    return QG_OK;
}
