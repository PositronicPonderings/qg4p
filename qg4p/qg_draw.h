/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    qg_draw.h
 * @brief   Drawing primitives, QuickBasic style.
 *
 * LAYER:   Public API (drawing)
 * DEPENDS: qg_screen.h
 *
 * COMMON RULES FOR EVERY FUNCTION HERE
 *   - Coordinates are pixels. (0,0) is the top-left corner. Shapes may extend
 *     off-screen; they're clipped safely.
 *   - Colours:  a palette index 0..254, or
 *               QG_DEFAULT      -> the screen's foreground colour
 *               QG_TRANSPARENT  -> "don't draw this part"
 *     For shapes with an outline and an interior, passing QG_TRANSPARENT as
 *     the fill gives an outline only; as the stroke, a filled shape with no
 *     outline.
 *   - Stroke thickness comes from qg_screen_set_line_width() (default 1).
 *
 * QUICKBASIC EQUIVALENTS
 *   CLS                          qg_cls(scr, color)
 *   PSET (x, y), c               qg_pset(scr, x, y, c)
 *   PRESET (x, y)                qg_preset(scr, x, y, QG_DEFAULT)
 *   LINE (x1,y1)-(x2,y2), c      qg_line(scr, x1, y1, x2, y2, c)
 *   LINE ..., , , &HF0F0         qg_screen_set_line_style(scr, 0xF0F0)
 *   VIEW (x1,y1)-(x2,y2)         qg_view(scr, x1, y1, x2, y2, true)
 *   GET / PUT                    qg_get / qg_put            (qg_block.h)
 *   LINE (x1,y1)-(x2,y2), c, B   qg_box(scr, x1, y1, x2, y2, c, QG_TRANSPARENT)
 *   LINE (x1,y1)-(x2,y2), c, BF  qg_box(scr, x1, y1, x2, y2, c, c)
 *   CIRCLE (x, y), r, c          qg_circle(scr, x, y, r, c, QG_TRANSPARENT)
 *   CIRCLE ... , aspect          qg_ellipse(scr, x, y, rx, ry, c, fill)
 *   CIRCLE ... , start, end      qg_arc(scr, x, y, rx, ry, start, end, c)
 *   POINT (x, y)                 qg_point(scr, x, y)              (BUF8 only)
 *   PAINT (x, y), fill, border   qg_paint(scr, x, y, fill, border) (BUF8 only)
 *
 * FRAMEBUFFER-ONLY (BUF8 screens)
 *   POINT (x, y)                 qg_point(scr, x, y)
 *   PAINT (x, y), fill, border   qg_paint(scr, x, y, fill, border)
 *   These need to READ pixels back, which only a framebuffer allows.
 */
#ifndef QG_DRAW_H
#define QG_DRAW_H

#include "qg_screen.h"

/* -------------------------------------------------------------------------- */
/*  The view (QuickBasic: VIEW)                                               */
/* -------------------------------------------------------------------------- */

/**
 * Restrict all drawing to a rectangle of the screen. QuickBasic: VIEW
 *
 * Corners are screen pixels, in any order, both included. Everything that
 * draws is cut off at the view's edges: shapes, text, images, PAINT, PUT.
 *
 * @param move_origin
 *   false: coordinates stay screen coordinates; the view only clips.
 *          (QuickBasic: VIEW SCREEN)
 *   true:  (0, 0) becomes the view's top-left corner, and percentages
 *          (the _pct functions) measure the view instead of the screen.
 *          Handy for drawing a panel's contents without caring where the
 *          panel is.                                   (QuickBasic: VIEW)
 *
 * While a view smaller than the screen is set:
 *   - qg_cls() clears just the view;
 *   - text wraps at the view's right edge;
 *   - cursor printing doesn't scroll (text is cut off at the bottom).
 */
void qg_view(qg_screen_t *scr, int16_t x1, int16_t y1, int16_t x2, int16_t y2,
             bool move_origin);

/** Back to the whole screen, origin at its corner. QuickBasic: VIEW */
void qg_view_reset(qg_screen_t *scr);

/** Size of the current coordinate space: the view's size if its origin was
 *  moved, otherwise the screen's. Percentages are measured against this. */
int16_t qg_view_width(const qg_screen_t *scr);
int16_t qg_view_height(const qg_screen_t *scr);

/**
 * Clear the view (the whole screen unless qg_view() set one) and move the
 * print cursor home (0,0).
 * QG_DEFAULT (or QG_TRANSPARENT) clears to the screen's background colour.
 */
void qg_cls(qg_screen_t *scr, qg_color_t color);

/** Set one pixel. Always 1 pixel, whatever the line width. */
void qg_pset(qg_screen_t *scr, int16_t x, int16_t y, qg_color_t color);

/**
 * Set one pixel, in the BACKGROUND colour unless a colour is given.
 * QuickBasic: PRESET (x, y) [, color]
 */
void qg_preset(qg_screen_t *scr, int16_t x, int16_t y, qg_color_t color);

/**
 * Set the pattern for lines and box outlines. QuickBasic: LINE's style.
 * 16 bits, read from the most significant bit: 1 = draw, 0 = skip, repeated
 * every 16 pixels. 0xFFFF (the default) is solid; 0xF0F0 dashed; 0xAAAA
 * dotted. 0 also means solid. Works at any line width. Circles, ellipses
 * and arcs stay solid.
 */
void qg_screen_set_line_style(qg_screen_t *scr, uint16_t pattern);

/** Straight line between two points (both ends included). */
void qg_line(qg_screen_t *scr, int16_t x1, int16_t y1,
              int16_t x2, int16_t y2, qg_color_t color);

/**
 * Box between two opposite corners (both included, any order).
 * @param stroke  outline colour (drawn inward, line-width thick)
 * @param fill    interior colour
 */
void qg_box(qg_screen_t *scr, int16_t x1, int16_t y1, int16_t x2, int16_t y2,
             qg_color_t stroke, qg_color_t fill);

/** Circle of radius r around (cx, cy). Its full width is 2r + 1 pixels. */
void qg_circle(qg_screen_t *scr, int16_t cx, int16_t cy, int16_t r,
                qg_color_t stroke, qg_color_t fill);

/** Ellipse with horizontal radius rx and vertical radius ry. */
void qg_ellipse(qg_screen_t *scr, int16_t cx, int16_t cy, int16_t rx, int16_t ry,
                 qg_color_t stroke, qg_color_t fill);

/**
 * Part of an ellipse outline (use rx == ry for a circular arc).
 *
 * ANGLES are in degrees, measured the way QuickBasic and maths do:
 * 0 = 3 o'clock, 90 = 12 o'clock, 180 = 9 o'clock, 270 = 6 o'clock.
 * The arc runs COUNTER-CLOCKWISE from start to end, so
 *     qg_arc(scr, x, y, r, r, 0, 90, c)    is the top-right quarter
 *     qg_arc(scr, x, y, r, r, 90, 0, c)    is the other three quarters
 * Negative angles and angles over 360 are allowed. start == end draws the
 * whole outline.
 */
void qg_arc(qg_screen_t *scr, int16_t cx, int16_t cy, int16_t rx, int16_t ry,
             int16_t start_deg, int16_t end_deg, qg_color_t color);

/**
 * Read one pixel's colour (palette index). QuickBasic: POINT
 * @return 0..255, or QG_NONE on a DIRECT screen or off-screen.
 */
qg_color_t qg_point(qg_screen_t *scr, int16_t x, int16_t y);

/**
 * Flood-fill the area around (x, y). QuickBasic: PAINT
 *
 *   border = a colour:   fill outward until reaching pixels of that colour
 *                        (QuickBasic's PAINT (x, y), fill, border)
 *   border = QG_DEFAULT: fill the connected area that has the same colour
 *                        as the starting pixel (a "bucket fill")
 *
 * "Connected" means touching left, right, up or down (not diagonally), so a
 * 1-pixel-wide diagonal line still holds the paint in.
 *
 * @return QG_OK; QG_ERR_UNSUPPORTED on DIRECT screens; QG_ERR_ARG if
 *         (x, y) is off-screen; QG_ERR_OVERFLOW if the shape was too
 *         complex for QG_PAINT_STACK (the fill is then incomplete).
 */
qg_err_t qg_paint(qg_screen_t *scr, int16_t x, int16_t y,
                    qg_color_t fill, qg_color_t border);

#endif /* QG_DRAW_H */
