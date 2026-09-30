/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    qg_draw_pct.c
 * @brief   Relative-coordinate wrappers around qg_draw.c.
 *
 * LAYER:   Public API (drawing)
 * DEPENDS: qg_draw_pct.h
 *
 * THE ONE FORMULA
 *   pixel = pct * (size - 1) / 100
 *
 * Using (size - 1) rather than size makes 100% land ON the last pixel
 * instead of one pixel past the edge, so qg_box_pct(scr, 0, 0, 100, 100, ...)
 * covers exactly the whole screen. The multiplication happens before the
 * division, so no precision is lost to rounding part-way through.
 */
#include "qg_draw_pct.h"

static inline int16_t scale(int32_t size, uint8_t pct)
{
    return (int16_t)(((int32_t)pct * (size - 1)) / 100);
}

/* Percentages measure the current coordinate space: the screen, or the
 * view if qg_view() moved the origin to it.                                  */
int16_t qg_pct_x(const qg_screen_t *scr, uint8_t pct)
{
    return scale(qg_view_width(scr), pct);
}

int16_t qg_pct_y(const qg_screen_t *scr, uint8_t pct)
{
    return scale(qg_view_height(scr), pct);
}

int16_t qg_pct_r(const qg_screen_t *scr, uint8_t pct)
{
    int32_t w = qg_view_width(scr), h = qg_view_height(scr);
    return scale(w < h ? w : h, pct);
}

void qg_view_pct(qg_screen_t *scr, uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2,
                 bool move_origin)
{
    if (scr == NULL) return;
    /* A view is always placed on the SCREEN, so measure against the screen. */
    qg_view(scr, scale(scr->width, x1), scale(scr->height, y1),
                 scale(scr->width, x2), scale(scr->height, y2), move_origin);
}

/* -------------------------------------------------------------------------- */

void qg_pset_pct(qg_screen_t *scr, uint8_t x, uint8_t y, qg_color_t color)
{
    if (scr == NULL) return;
    qg_pset(scr, qg_pct_x(scr, x), qg_pct_y(scr, y), color);
}

void qg_line_pct(qg_screen_t *scr, uint8_t x1, uint8_t y1,
                  uint8_t x2, uint8_t y2, qg_color_t color)
{
    if (scr == NULL) return;
    qg_line(scr, qg_pct_x(scr, x1), qg_pct_y(scr, y1),
                  qg_pct_x(scr, x2), qg_pct_y(scr, y2), color);
}

void qg_box_pct(qg_screen_t *scr, uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2,
                 qg_color_t stroke, qg_color_t fill)
{
    if (scr == NULL) return;
    qg_box(scr, qg_pct_x(scr, x1), qg_pct_y(scr, y1),
                 qg_pct_x(scr, x2), qg_pct_y(scr, y2), stroke, fill);
}

void qg_circle_pct(qg_screen_t *scr, uint8_t cx, uint8_t cy, uint8_t r,
                    qg_color_t stroke, qg_color_t fill)
{
    if (scr == NULL) return;
    qg_circle(scr, qg_pct_x(scr, cx), qg_pct_y(scr, cy), qg_pct_r(scr, r),
               stroke, fill);
}

void qg_ellipse_pct(qg_screen_t *scr, uint8_t cx, uint8_t cy, uint8_t rx, uint8_t ry,
                     qg_color_t stroke, qg_color_t fill)
{
    if (scr == NULL) return;
    qg_ellipse(scr, qg_pct_x(scr, cx), qg_pct_y(scr, cy),
                qg_pct_x(scr, rx), qg_pct_y(scr, ry), stroke, fill);
}

void qg_arc_pct(qg_screen_t *scr, uint8_t cx, uint8_t cy, uint8_t rx, uint8_t ry,
                 int16_t start_deg, int16_t end_deg, qg_color_t color)
{
    if (scr == NULL) return;
    qg_arc(scr, qg_pct_x(scr, cx), qg_pct_y(scr, cy),
            qg_pct_x(scr, rx), qg_pct_y(scr, ry), start_deg, end_deg, color);
}
