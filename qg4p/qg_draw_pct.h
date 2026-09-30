/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    qg_draw_pct.h
 * @brief   Drawing with RELATIVE coordinates: percentages of the screen.
 *
 * LAYER:   Public API (drawing)
 * DEPENDS: qg_draw.h
 *
 * Every function here is a thin wrapper: it converts its percentages to
 * pixels and calls the absolute version in qg_draw.h. All the real drawing
 * logic lives in one place, so the two can never behave differently.
 *
 *     qg_circle_pct(scr, 50, 50, 25, QG_WHITE, QG_RED);
 *         -> a circle in the middle of the screen, a quarter of its size,
 *            on ANY screen size and in ANY rotation.
 *
 * HOW PERCENTAGES MAP TO PIXELS
 *   x:       0 = the leftmost pixel, 100 = the rightmost pixel
 *   y:       0 = the top row,        100 = the bottom row
 *   radius:  a percentage of the screen's SMALLER dimension, so a circle
 *            centred at (50, 50) with radius 50 just touches the nearest
 *            edges, whichever way round the screen is.
 *   rx, ry:  (ellipses and arcs) a percentage of the width and the height
 *            respectively, so an ellipse (50, 50, 50, 50) touches all four
 *            edges.
 *
 *   Percentages are whole numbers, 0..255. Values over 100 are allowed and
 *   land off-screen, where clipping trims them as usual.
 *
 *   Conversion rounds DOWN: 50% of a 240-pixel-wide screen is pixel 119
 *   (the pixel just left of centre; there is no exact middle pixel when the
 *   width is even).
 */
#ifndef QG_DRAW_PCT_H
#define QG_DRAW_PCT_H

#include "qg_draw.h"

/* -------------------------------------------------------------------------- */
/*  Conversion helpers - also useful on their own                             */
/* -------------------------------------------------------------------------- */

/** Percentage of the width  -> x pixel (0..width-1 for 0..100). */
int16_t qg_pct_x(const qg_screen_t *scr, uint8_t pct);

/** Percentage of the height -> y pixel (0..height-1 for 0..100). */
int16_t qg_pct_y(const qg_screen_t *scr, uint8_t pct);

/** Percentage of the smaller dimension -> pixels (for radii and sizes). */
int16_t qg_pct_r(const qg_screen_t *scr, uint8_t pct);

/* The helpers above measure the current coordinate space: the screen, or the
 * view if qg_view() was called with move_origin, so a panel can be laid out
 * in percentages of itself.                                                 */

/** qg_view() with the corners as percentages of the SCREEN. */
void qg_view_pct(qg_screen_t *scr, uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2,
                 bool move_origin);

/* -------------------------------------------------------------------------- */
/*  Relative versions of the primitives (same colour rules as qg_draw.h)     */
/* -------------------------------------------------------------------------- */

void qg_pset_pct(qg_screen_t *scr, uint8_t x, uint8_t y, qg_color_t color);

void qg_line_pct(qg_screen_t *scr, uint8_t x1, uint8_t y1,
                  uint8_t x2, uint8_t y2, qg_color_t color);

void qg_box_pct(qg_screen_t *scr, uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2,
                 qg_color_t stroke, qg_color_t fill);

/** r is a percentage of the smaller screen dimension. */
void qg_circle_pct(qg_screen_t *scr, uint8_t cx, uint8_t cy, uint8_t r,
                    qg_color_t stroke, qg_color_t fill);

/** rx is a percentage of the width, ry of the height. */
void qg_ellipse_pct(qg_screen_t *scr, uint8_t cx, uint8_t cy, uint8_t rx, uint8_t ry,
                     qg_color_t stroke, qg_color_t fill);

/** rx is a percentage of the width, ry of the height. Angles as qg_arc(). */
void qg_arc_pct(qg_screen_t *scr, uint8_t cx, uint8_t cy, uint8_t rx, uint8_t ry,
                 int16_t start_deg, int16_t end_deg, qg_color_t color);

#endif /* QG_DRAW_PCT_H */
