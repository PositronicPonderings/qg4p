/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    qg_palette.h
 * @brief   The standard 256-colour palette and named-colour helpers.
 *
 * LAYER:   Public API (colour)
 * DEPENDS: qg_types.h
 *
 * STANDARD PALETTE LAYOUT
 *   0..15     QuickBasic 16 colours (QG_BLACK ... QG_WHITE)
 *   16..231   6x6x6 colour cube. Index = 16 + 36*r + 6*g + b, each 0..5,
 *             mapping to levels 0, 51, 102, 153, 204, 255.
 *   232..254  23-step grey ramp, dark to light
 *   255       reserved: QG_TRANSPARENT. Stored as bright magenta so that if
 *             it is ever drawn by mistake, the bug is obvious on screen.
 *
 * Each screen gets its own *copy* of this palette at init (plan section 5),
 * so one screen's palette changes never affect the other.
 *
 * CHANGING A SCREEN'S PALETTE
 *   qg_palette_set() changes one entry of one screen's palette.
 *   - DIRECT screens: only drawing done AFTER the change uses the new colour;
 *     pixels already on the glass keep the colour they were drawn with.
 *   - BUF8 screens: everything using that entry changes at the next flush.
 *     This is what makes palette animation effects possible: change a few
 *     entries, flush, and whole areas change colour without being redrawn.
 */
#ifndef QG_PALETTE_H
#define QG_PALETTE_H

#include <stddef.h>
#include "qg_types.h"

typedef struct qg_screen qg_screen_t;   /* defined in qg_screen.h */

/** Copy the standard palette (as RGB565) into a 256-entry table. */
void qg_palette_copy_standard(uint16_t dst[256]);

/** Read-only access to the standard palette (RGB565). */
const uint16_t *qg_palette_standard(void);

/**
 * Name of a named colour ("RED", "LIGHTCYAN", ...), or NULL if `c` is not
 * one of the 16 named colours. (qg_color_from_name() goes the other way,
 * and is what {c:NAME} markup uses.)
 */
const char *qg_color_name(qg_color_t c);

/**
 * Read a palette entry back as 8-bit red, green and blue, exactly as the
 * screen stores it. (With a colour adjustment set, what's SENT to the panel
 * differs: this returns the colour you asked for, not the adjusted one.) Palettes hold RGB565, which keeps 5 bits
 * of red and blue and 6 of green, so values come back slightly rounded:
 * QuickBasic's RED (170, 0, 0) reads as (173, 0, 0).
 *
 *     uint8_t r, g, b;
 *     qg_palette_get(scr.palette, QG_RED, &r, &g, &b);
 *
 * @param palette  a screen's palette (scr.palette) or qg_palette_standard()
 */
void qg_palette_get(const uint16_t *palette, qg_color_t index,
                    uint8_t *r, uint8_t *g, uint8_t *b);

/**
 * Find the palette colour closest to an RGB colour. Do this once, in setup
 * code, and keep the result: it searches all 255 usable entries.
 *
 *     qg_color_t gold = qg_color_from_rgb(212, 175, 55, NULL);
 *     qg_color_t mine = qg_color_from_rgb(212, 175, 55, scr.palette);
 *
 * @param palette  a 256-entry RGB565 palette (e.g. scr.palette), or NULL for
 *                 the standard palette
 * @return         the nearest palette index, 0..254 (never QG_TRANSPARENT)
 */
qg_color_t qg_color_from_rgb(uint8_t r, uint8_t g, uint8_t b, const uint16_t *palette);

/**
 * Change one entry of a screen's palette (0..255).
 *
 * Entry 255 is special: no drawing function ever draws with it (as a colour
 * it means QG_TRANSPARENT). But on a framebuffer screen, qg_put()'s bitwise
 * modes can leave the NUMBER 255 in a pixel, and that pixel then shows
 * entry 255's colour (magenta, until you change it here).
 *
 * TIP: to create a custom colour, pick an entry you're not using, e.g. one
 * of the grey ramp (232..254), and give it your RGB value:
 *     qg_palette_set(&scr, 240, 212, 175, 55);   // index 240 is now gold
 *
 * @return QG_OK, or QG_ERR_ARG for a NULL screen or an index above 255
 */
qg_err_t qg_palette_set(qg_screen_t *scr, qg_color_t index,
                          uint8_t r, uint8_t g, uint8_t b);

/** Restore a screen's palette to the standard palette. */
void qg_palette_reset(qg_screen_t *scr);

/**
 * Correct a panel's colours: a brightness (gain) and a mid-tone curve
 * (gamma) for each of red, green and blue, applied to everything the screen
 * sends. Pass NULL as `adj` to switch it off.
 *
 *     static qg_color_adjust_state_t adj_state;          // 1,280 bytes: yours
 *     qg_color_adjust_t adj = { .gain = { 100, 100, 90 },      // blue: 90 %
 *                               .gamma = { 100, 100, 140 } };  // darker blue mid-tones
 *     qg_screen_set_color_adjust(&scr, &adj, &adj_state);
 *
 * The working tables live in storage YOU provide (one per adjusted screen,
 * kept for as long as the adjustment is on), so screens that aren't
 * adjusted cost nothing at all.
 *
 * WHY: cheap panels differ. A TN panel's blue may come up brighter in the
 * mid-tones than its red and green, so greys look blue and whites look
 * cold. Nudging the blue gamma up (and the gain down a little) fixes it for
 * that panel, and only that panel: each screen has its own adjustment.
 *
 * HOW TO FIND THE NUMBERS: run examples/calibrate.c and tune them live
 * over USB serial while watching a grey ramp.
 *
 *   gain   percent, 0..100: how bright full intensity is. Lower one channel
 *          to take a tint out of white.
 *   gamma  x 100, 50..300: 100 leaves mid-tones alone; higher makes them
 *          darker (and that channel's influence on mixed colours smaller);
 *          lower makes them brighter. Black and full intensity don't move.
 *
 * The adjustment happens where colours leave for the panel, so everything
 * else (qg_palette_get, qg_color_from_rgb, image colour matching) still
 * works with the colours you asked for. It costs no time per pixel.
 * On a framebuffer screen the next flush sends everything re-coloured;
 * on a DIRECT screen, redraw to see the change.
 *
 * @return QG_OK, or QG_ERR_ARG for a missing state, a gain above 100, or a
 *         gamma outside 50..300
 */
qg_err_t qg_screen_set_color_adjust(qg_screen_t *scr, const qg_color_adjust_t *adj,
                                    qg_color_adjust_state_t *state);

/**
 * Look up a named colour ("RED", "lightcyan", ...; case doesn't matter).
 * `len` is the name's length, so it can point into a longer string.
 * @return the colour 0..15, or -1 if the name isn't one of the 16.
 */
int qg_color_from_name(const char *name, size_t len);

#endif /* QG_PALETTE_H */
