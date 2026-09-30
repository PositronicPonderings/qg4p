/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    qg_draw.c
 * @brief   Drawing primitives and the clipping gateway to the backends.
 *
 * LAYER:   Public API (drawing)
 * DEPENDS: qg_draw.h, qg_internal.h, <math.h> (thick lines, arcs)
 *
 * ---------------------------------------------------------------------------
 *  ONE IDEA RUNS THROUGH THIS WHOLE FILE: DRAW IN HORIZONTAL SPANS
 * ---------------------------------------------------------------------------
 *  On a DIRECT screen, every separate piece we draw costs a little setup on
 *  the SPI bus (select the chip, set a window, send the pixels). Sending
 *  pixels one at a time pays that cost for every pixel. So wherever possible,
 *  shapes are broken into horizontal runs ("spans") of the same colour and
 *  each span is sent as one small rectangle:
 *
 *    - Filled shapes: one span per row.
 *    - Circle and ellipse outlines: at most two spans per row.
 *    - Lines: consecutive pixels in the same row (or column) are grouped.
 *
 *  Only arcs fall back to testing individual pixels, and even then adjacent
 *  pixels are grouped back into spans before they're sent.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "qg_draw.h"
#include "qg_internal.h"

/* ========================================================================== */
/*  Internal gateway                                                          */
/* ========================================================================== */

void qg_int_fill_rect(qg_screen_t *scr, int32_t x, int32_t y,
                       int32_t w, int32_t h, qg_color_t color)
{
    if (scr == NULL || !scr->ready || w <= 0 || h <= 0) {
        return;
    }

    /* 1 & 2: special colour values. */
    if (color == QG_TRANSPARENT) {
        return;
    }
    if (color == QG_DEFAULT) {
        color = scr->bg_color;
    }
    if (color > 254) {
        return;                     /* not a valid palette index: ignore */
    }

    /*
     * 3: THE VIEW
     * Callers' coordinates are relative to the origin, which qg_view() may
     * have moved to a viewport's corner; adding it gives screen pixels.
     * Then clip to the view (the whole screen unless qg_view() set one).
     * Right/bottom edges are "exclusive" (one past the last pixel), because
     * that makes the maths simple: width = x2 - x1, and empty means x2 <= x1.
     */
    int32_t x1 = x + scr->origin_x;
    int32_t y1 = y + scr->origin_y;
    int32_t x2 = x1 + w;
    int32_t y2 = y1 + h;

    if (x1 < scr->view_x0) x1 = scr->view_x0;
    if (y1 < scr->view_y0) y1 = scr->view_y0;
    if (x2 > scr->view_x1) x2 = scr->view_x1;
    if (y2 > scr->view_y1) y2 = scr->view_y1;

    if (x2 <= x1 || y2 <= y1) {
        return;                     /* entirely off-screen */
    }

    /* 4: hand over to whichever backend this screen uses (screen pixels). */
    scr->backend->fill_rect(scr, (int16_t)x1, (int16_t)y1,
                            (int16_t)(x2 - x1), (int16_t)(y2 - y1), color);
}

/* ========================================================================== */
/*  Small helpers                                                             */
/* ========================================================================== */

/** QG_DEFAULT -> the screen's foreground colour; anything else unchanged. */
static inline qg_color_t fg_or(const qg_screen_t *scr, qg_color_t c)
{
    return (c == QG_DEFAULT) ? scr->fg_color : c;
}

/** True if this colour would actually put pixels on the screen. */
static inline bool visible(qg_color_t c)
{
    return c <= 254;
}

/** Horizontal span from xa to xb (inclusive, either order) on row y. */
static inline void span(qg_screen_t *scr, int32_t xa, int32_t xb, int32_t y,
                        qg_color_t c)
{
    if (xb < xa) {
        int32_t t = xa; xa = xb; xb = t;
    }
    qg_int_fill_rect(scr, xa, y, xb - xa + 1, 1, c);
}

/**
 * Integer square root: the largest r with r*r <= n.
 *
 * This is the classic "digit by digit" method, the binary version of long-hand
 * square roots from school. It works two bits at a time from the top, uses
 * only shifts, adds and compares, and gives an exact answer with no floating
 * point.
 */
static uint32_t isqrt64(uint64_t n)
{
    uint64_t result = 0;
    uint64_t bit    = (uint64_t)1 << 62;   /* highest power of 4 in 64 bits */

    while (bit > n) {
        bit >>= 2;
    }
    while (bit != 0) {
        if (n >= result + bit) {
            n      -= result + bit;
            result  = (result >> 1) + bit;
        } else {
            result >>= 1;
        }
        bit >>= 2;
    }
    return (uint32_t)result;
}

/* ========================================================================== */
/*  The view                                                                  */
/* ========================================================================== */

void qg_view(qg_screen_t *scr, int16_t x1, int16_t y1, int16_t x2, int16_t y2,
             bool move_origin)
{
    if (scr == NULL) return;
    int32_t l = (x1 < x2) ? x1 : x2, r = (x1 < x2) ? x2 : x1;
    int32_t t = (y1 < y2) ? y1 : y2, b = (y1 < y2) ? y2 : y1;

    /* Keep the view on the screen. */
    if (l < 0) l = 0;
    if (t < 0) t = 0;
    if (r > scr->width - 1)  r = scr->width - 1;
    if (b > scr->height - 1) b = scr->height - 1;
    if (r < l || b < t) {                 /* entirely off-screen: an empty view */
        r = l - 1;
        b = t - 1;
    }

    scr->view_x0 = (int16_t)l;      scr->view_y0 = (int16_t)t;
    scr->view_x1 = (int16_t)(r + 1); scr->view_y1 = (int16_t)(b + 1);
    scr->view_moved = move_origin;
    scr->origin_x = move_origin ? (int16_t)l : 0;
    scr->origin_y = move_origin ? (int16_t)t : 0;
}

void qg_view_reset(qg_screen_t *scr)
{
    if (scr == NULL) return;
    scr->view_x0 = 0;           scr->view_y0 = 0;
    scr->view_x1 = scr->width;  scr->view_y1 = scr->height;
    scr->origin_x = 0;          scr->origin_y = 0;
    scr->view_moved = false;
}

int16_t qg_view_width(const qg_screen_t *scr)
{
    return scr->view_moved ? (int16_t)(scr->view_x1 - scr->view_x0) : scr->width;
}

int16_t qg_view_height(const qg_screen_t *scr)
{
    return scr->view_moved ? (int16_t)(scr->view_y1 - scr->view_y0) : scr->height;
}

/* ========================================================================== */
/*  CLS and PSET                                                              */
/* ========================================================================== */

void qg_cls(qg_screen_t *scr, qg_color_t color)
{
    if (scr == NULL || !scr->ready) {
        return;
    }
    if (color == QG_TRANSPARENT) {
        color = QG_DEFAULT;
    }
    /* Clears the view: the whole screen unless qg_view() set a smaller one
     * (QuickBasic's CLS does the same).                                     */
    qg_int_fill_rect(scr, qg_int_left(scr), qg_int_top(scr),
                     qg_int_right(scr) - qg_int_left(scr),
                     qg_int_bottom(scr) - qg_int_top(scr), color);
    scr->cursor_x = 0;
    scr->cursor_y = 0;
    scr->margin_x = 0;
    scr->hist_count = 0;          /* nothing left on screen to scroll */
    scr->hist_open  = false;
}

void qg_pset(qg_screen_t *scr, int16_t x, int16_t y, qg_color_t color)
{
    if (scr == NULL) return;
    qg_int_fill_rect(scr, x, y, 1, 1, fg_or(scr, color));
}

void qg_preset(qg_screen_t *scr, int16_t x, int16_t y, qg_color_t color)
{
    if (scr == NULL) return;
    /* Like PSET, but "no colour given" means the BACKGROUND colour: the
     * classic QuickBasic way to erase a point.                             */
    qg_int_fill_rect(scr, x, y, 1, 1, (color == QG_DEFAULT) ? scr->bg_color : color);
}

/* ========================================================================== */
/*  Lines                                                                     */
/* ========================================================================== */

/*
 * THIN LINES: BRESENHAM'S ALGORITHM, SENT AS RUNS
 *
 * Bresenham's algorithm steps along a line one pixel at a time using only
 * integer adds. `err` tracks how far the drawn pixels have drifted from the
 * true line; whenever it builds up enough, we step in the other direction.
 *
 * A "mostly horizontal" line (x-major) moves right on EVERY step and only
 * sometimes down, so it naturally comes out as horizontal runs:
 *
 *      ####
 *          ####
 *              ####
 *
 * We collect each run and send it as one rectangle instead of 4 separate
 * pixels. A mostly vertical line (y-major) is grouped into vertical runs.
 */
static void line_thin(qg_screen_t *scr, int32_t x1, int32_t y1,
                      int32_t x2, int32_t y2, qg_color_t c)
{
    int32_t dx  =  abs(x2 - x1), sx = (x1 < x2) ? 1 : -1;
    int32_t dy  = -abs(y2 - y1), sy = (y1 < y2) ? 1 : -1;
    int32_t err = dx + dy;
    bool    x_major = dx >= -dy;

    int32_t x = x1, y = y1;
    int32_t run_x = x, run_y = y, run_len = 0;

    for (;;) {
        /* Does this pixel continue the current run? For an x-major line the
         * run is broken when y changes; for y-major, when x changes.        */
        bool breaks = run_len > 0 && (x_major ? (y != run_y) : (x != run_x));
        if (breaks) {
            if (x_major) {
                int32_t left = (sx > 0) ? run_x : run_x - run_len + 1;
                qg_int_fill_rect(scr, left, run_y, run_len, 1, c);
            } else {
                int32_t top = (sy > 0) ? run_y : run_y - run_len + 1;
                qg_int_fill_rect(scr, run_x, top, 1, run_len, c);
            }
            run_len = 0;
        }
        if (run_len == 0) {
            run_x = x;
            run_y = y;
        }
        run_len++;

        if (x == x2 && y == y2) {
            break;
        }
        int32_t e2 = 2 * err;
        if (e2 >= dy) { err += dy; x += sx; }
        if (e2 <= dx) { err += dx; y += sy; }
    }

    /* Send the final run. */
    if (x_major) {
        int32_t left = (sx > 0) ? run_x : run_x - run_len + 1;
        qg_int_fill_rect(scr, left, run_y, run_len, 1, c);
    } else {
        int32_t top = (sy > 0) ? run_y : run_y - run_len + 1;
        qg_int_fill_rect(scr, run_x, top, 1, run_len, c);
    }
}

/*
 * FILL A CONVEX POLYGON, ONE SCANLINE AT A TIME
 *
 * "Convex" means no dents: every horizontal line crosses the outline at most
 * twice. So for each pixel row we find where the row's centre line crosses
 * each edge, take the leftmost and rightmost crossings, and fill between.
 *
 * Coordinates are continuous: pixel (i, j) covers the square from (i, j) to
 * (i+1, j+1), and its centre is at (i + 0.5, j + 0.5). A pixel is filled when
 * its centre is inside the polygon. A centre lying exactly on the top or left
 * edge counts as inside; on the bottom or right edge, outside. That way two
 * shapes sharing an edge never both claim the same pixels.
 */
static void fill_convex(qg_screen_t *scr, const float *px, const float *py,
                        int n, qg_color_t c)
{
    float ymin = py[0], ymax = py[0];
    for (int i = 1; i < n; i++) {
        if (py[i] < ymin) ymin = py[i];
        if (py[i] > ymax) ymax = py[i];
    }

    int32_t row0 = (int32_t)floorf(ymin);
    int32_t row1 = (int32_t)ceilf(ymax) - 1;
    if (row0 < qg_int_top(scr))        row0 = qg_int_top(scr);     /* skip rows */
    if (row1 > qg_int_bottom(scr) - 1) row1 = qg_int_bottom(scr) - 1; /* not seen */

    for (int32_t row = row0; row <= row1; row++) {
        float yc = (float)row + 0.5f;
        float xl =  1e30f;
        float xr = -1e30f;

        for (int i = 0; i < n; i++) {
            int   j  = (i + 1) % n;
            float ya = py[i], yb = py[j];
            /* Does this edge cross the row's centre line? The half-open test
             * (>= one end, < the other) counts a shared corner only once.  */
            if ((yc >= ya && yc < yb) || (yc >= yb && yc < ya)) {
                float x = px[i] + (yc - ya) * (px[j] - px[i]) / (yb - ya);
                if (x < xl) xl = x;
                if (x > xr) xr = x;
            }
        }
        if (xl <= xr) {
            /* Pixel centres in [xl, xr): the left edge counts, the right edge
             * doesn't. That's the same half-open rule used for rows above, and
             * it's what makes a 4-pixel-thick line exactly 4 pixels wide even
             * when its edges fall precisely on pixel centres.               */
            int32_t c0 = (int32_t)ceilf(xl - 0.5f);
            int32_t c1 = (int32_t)ceilf(xr - 0.5f) - 1;
            if (c1 >= c0) {
                span(scr, c0, c1, row, c);
            }
        }
    }
}

/*
 * THICK LINES: A ROTATED RECTANGLE
 *
 * A line `t` pixels thick is a rectangle centred on the line. We find its
 * four corners by stepping t/2 to either side of each end point, at right
 * angles to the line (the "normal" direction), and fill it as a polygon.
 *
 *        corner 0 +--------------------------+ corner 1
 *                 |  P1 ------------------ P2 |   <- line through the middle
 *        corner 3 +--------------------------+ corner 2
 *
 * The ends are extended by half a pixel so both end pixels are fully covered,
 * matching what a 1-pixel line does.
 */
static void line_thick(qg_screen_t *scr, int32_t x1, int32_t y1,
                       int32_t x2, int32_t y2, int32_t t, qg_color_t c)
{
    float ax = (float)x1 + 0.5f, ay = (float)y1 + 0.5f;   /* pixel centres */
    float bx = (float)x2 + 0.5f, by = (float)y2 + 0.5f;
    float dx = bx - ax, dy = by - ay;
    float len = sqrtf(dx * dx + dy * dy);

    if (len < 0.001f) {
        /* Both ends are the same pixel: draw a t x t square around it. */
        qg_int_fill_rect(scr, x1 - t / 2, y1 - t / 2, t, t, c);
        return;
    }

    float ux = dx / len, uy = dy / len;       /* unit vector along the line */
    float half = (float)t * 0.5f;
    float nx = -uy * half, ny = ux * half;    /* t/2 at right angles to it  */

    ax -= ux * 0.5f;  ay -= uy * 0.5f;        /* extend each end by 1/2 px  */
    bx += ux * 0.5f;  by += uy * 0.5f;

    const float px[4] = { ax + nx, bx + nx, bx - nx, ax - nx };
    const float py[4] = { ay + ny, by + ny, by - ny, ay - ny };
    fill_convex(scr, px, py, 4, c);
}

/*
 * LINE STYLES (QuickBasic: LINE ..., , , style)
 *
 * A style is a 16-bit pattern, read from the most significant bit down: a 1
 * draws the pixel, a 0 skips it, and the pattern repeats every 16 pixels.
 *
 *      0xFFFF  ################   solid (the default)
 *      0xF0F0  ####....####....   dashes
 *      0xAAAA  #.#.#.#.#.#.#.#.   dots
 *      0xFF18  ########...##...   dash-dot
 *
 * Styled lines walk the same Bresenham path as solid ones. Each "on" stretch
 * becomes a run (thin lines) or a short thick segment (thick lines), so a
 * dashed thick line is a series of properly shaped dashes.
 */
static inline bool style_bit(uint16_t style, uint32_t k)
{
    return (style >> (15u - (k & 15u))) & 1u;
}

/*
 * One dash of a thick styled line, from pixel (sx, sy) to (ex, ey), drawn as
 * a rectangle along the WHOLE line's direction (ux, uy), extended by half a
 * pixel at each end so it covers exactly its own pixels. Using the whole
 * line's direction matters for one-pixel dashes (dots), which have no
 * direction of their own: drawn as squares, neighbouring dots would merge.
 */
static void dash_thick(qg_screen_t *scr, int32_t sx, int32_t sy, int32_t ex, int32_t ey,
                       float ux, float uy, int32_t t, qg_color_t c)
{
    float ax = (float)sx + 0.5f - ux * 0.5f, ay = (float)sy + 0.5f - uy * 0.5f;
    float bx = (float)ex + 0.5f + ux * 0.5f, by = (float)ey + 0.5f + uy * 0.5f;
    float half = (float)t * 0.5f;
    float nx = -uy * half, ny = ux * half;
    const float px[4] = { ax + nx, bx + nx, bx - nx, ax - nx };
    const float py[4] = { ay + ny, by + ny, by - ny, ay - ny };
    fill_convex(scr, px, py, 4, c);
}

static void line_styled(qg_screen_t *scr, int32_t x1, int32_t y1, int32_t x2, int32_t y2,
                        int32_t t, uint16_t style, qg_color_t c)
{
    /* The line's direction, for thick dashes (see dash_thick). */
    float fdx = (float)(x2 - x1), fdy = (float)(y2 - y1);
    float flen = sqrtf(fdx * fdx + fdy * fdy);
    float ux = (flen > 0.001f) ? fdx / flen : 1.0f, uy = (flen > 0.001f) ? fdy / flen : 0.0f;

    int32_t dx  =  abs(x2 - x1), sx = (x1 < x2) ? 1 : -1;
    int32_t dy  = -abs(y2 - y1), sy = (y1 < y2) ? 1 : -1;
    int32_t err = dx + dy;
    bool    x_major = dx >= -dy;

    int32_t  x = x1, y = y1;
    bool     in_run = false;
    int32_t  run_x = 0, run_y = 0, run_len = 0;   /* thin: the current run   */
    int32_t  seg_x = 0, seg_y = 0, end_x = 0, end_y = 0; /* thick: a dash    */

    for (uint32_t k = 0; ; k++) {
        bool on = style_bit(style, k);

        if (t <= 1) {
            /* Same run grouping as line_thin(), but an "off" pixel also ends
             * the run.                                                      */
            bool breaks = in_run && (!on || (x_major ? (y != run_y) : (x != run_x)));
            if (breaks) {
                if (x_major) qg_int_fill_rect(scr, (sx > 0) ? run_x : run_x - run_len + 1, run_y, run_len, 1, c);
                else         qg_int_fill_rect(scr, run_x, (sy > 0) ? run_y : run_y - run_len + 1, 1, run_len, c);
                in_run = false;
            }
            if (on) {
                if (!in_run) { run_x = x; run_y = y; run_len = 0; in_run = true; }
                run_len++;
            }
        } else {
            /* Thick: remember where each dash starts and ends, and draw it as
             * a short thick line once it's complete.                        */
            if (on) {
                if (!in_run) { seg_x = x; seg_y = y; in_run = true; }
                end_x = x; end_y = y;
            } else if (in_run) {
                dash_thick(scr, seg_x, seg_y, end_x, end_y, ux, uy, t, c);
                in_run = false;
            }
        }

        if (x == x2 && y == y2) break;
        int32_t e2 = 2 * err;
        if (e2 >= dy) { err += dy; x += sx; }
        if (e2 <= dx) { err += dx; y += sy; }
    }

    if (in_run) {                                           /* the last one */
        if (t > 1)        dash_thick(scr, seg_x, seg_y, end_x, end_y, ux, uy, t, c);
        else if (x_major) qg_int_fill_rect(scr, (sx > 0) ? run_x : run_x - run_len + 1, run_y, run_len, 1, c);
        else              qg_int_fill_rect(scr, run_x, (sy > 0) ? run_y : run_y - run_len + 1, 1, run_len, c);
    }
}

void qg_screen_set_line_style(qg_screen_t *scr, uint16_t pattern)
{
    if (scr != NULL) scr->line_style = pattern;
}

/* 0 and 0xFFFF both mean solid: a pattern with no pixels on would draw
 * nothing, which is never what anyone wants (and a zeroed screen structure
 * then behaves sensibly).                                                   */
static inline bool style_solid(uint16_t st) { return st == 0xFFFF || st == 0; }

void qg_line(qg_screen_t *scr, int16_t x1, int16_t y1,
             int16_t x2, int16_t y2, qg_color_t color)
{
    if (scr == NULL || !scr->ready) return;
    color = fg_or(scr, color);
    if (!visible(color)) return;

    if (!style_solid(scr->line_style)) {
        line_styled(scr, x1, y1, x2, y2, scr->line_width, scr->line_style, color);
    } else if (scr->line_width <= 1) {
        line_thin(scr, x1, y1, x2, y2, color);
    } else {
        line_thick(scr, x1, y1, x2, y2, scr->line_width, color);
    }
}

/* ========================================================================== */
/*  Boxes                                                                     */
/* ========================================================================== */

void qg_box(qg_screen_t *scr, int16_t x1, int16_t y1, int16_t x2, int16_t y2,
             qg_color_t stroke, qg_color_t fill)
{
    if (scr == NULL || !scr->ready) return;
    stroke = fg_or(scr, stroke);
    fill   = fg_or(scr, fill);

    /* Accept the corners in any order. */
    int32_t l = (x1 < x2) ? x1 : x2, r = (x1 < x2) ? x2 : x1;
    int32_t t = (y1 < y2) ? y1 : y2, b = (y1 < y2) ? y2 : y1;
    int32_t w = r - l + 1, h = b - t + 1;
    int32_t lw = scr->line_width;

    if (!visible(stroke)) {
        if (visible(fill)) qg_int_fill_rect(scr, l, t, w, h, fill);
        return;
    }

    /* Outline so thick that it meets in the middle: it's all outline. */
    if (2 * lw >= w || 2 * lw >= h) {
        qg_int_fill_rect(scr, l, t, w, h, stroke);
        return;
    }

    if (!style_solid(scr->line_style)) {
        /* A styled outline: the pattern runs once around the box, clockwise
         * from the top-left corner, carrying on around each corner. Each
         * "on" stretch along a side is one rectangle, lw deep, drawn inward. */
        uint16_t st = scr->line_style;
        uint32_t k  = 0;
        int32_t  run = -1, i;
        /* top: left to right */
        for (i = l; i <= r; i++, k++) {
            bool on = style_bit(st, k);
            if (on && run < 0) run = i;
            if (!on && run >= 0) { qg_int_fill_rect(scr, run, t, i - run, lw, stroke); run = -1; }
        }
        if (run >= 0) { qg_int_fill_rect(scr, run, t, r + 1 - run, lw, stroke); run = -1; }
        /* right: top to bottom, between the top and bottom edges */
        for (i = t + lw; i <= b - lw; i++, k++) {
            bool on = style_bit(st, k);
            if (on && run < 0) run = i;
            if (!on && run >= 0) { qg_int_fill_rect(scr, r - lw + 1, run, lw, i - run, stroke); run = -1; }
        }
        if (run >= 0) { qg_int_fill_rect(scr, r - lw + 1, run, lw, b - lw + 1 - run, stroke); run = -1; }
        /* bottom: right to left */
        for (i = r; i >= l; i--, k++) {
            bool on = style_bit(st, k);
            if (on && run < 0) run = i;
            if (!on && run >= 0) { qg_int_fill_rect(scr, i + 1, b - lw + 1, run - i, lw, stroke); run = -1; }
        }
        if (run >= 0) { qg_int_fill_rect(scr, l, b - lw + 1, run - l + 1, lw, stroke); run = -1; }
        /* left: bottom to top */
        for (i = b - lw; i >= t + lw; i--, k++) {
            bool on = style_bit(st, k);
            if (on && run < 0) run = i;
            if (!on && run >= 0) { qg_int_fill_rect(scr, l, i + 1, lw, run - i, stroke); run = -1; }
        }
        if (run >= 0) qg_int_fill_rect(scr, l, t + lw, lw, run - (t + lw) + 1, stroke);

        if (visible(fill)) {
            qg_int_fill_rect(scr, l + lw, t + lw, w - 2 * lw, h - 2 * lw, fill);
        }
        return;
    }

    /* Four sides, drawn inward. The left and right sides only cover the gap
     * between the top and bottom, so no pixel is sent twice.
     *
     *      TTTTTTTTTT
     *      L        R
     *      L  fill  R
     *      BBBBBBBBBB                                                        */
    qg_int_fill_rect(scr, l,          t,          w,  lw,          stroke);
    qg_int_fill_rect(scr, l,          b - lw + 1, w,  lw,          stroke);
    qg_int_fill_rect(scr, l,          t + lw,     lw, h - 2 * lw,  stroke);
    qg_int_fill_rect(scr, r - lw + 1, t + lw,     lw, h - 2 * lw,  stroke);

    if (visible(fill)) {
        qg_int_fill_rect(scr, l + lw, t + lw, w - 2 * lw, h - 2 * lw, fill);
    }
}

/* ========================================================================== */
/*  Circles, ellipses and arcs                                                */
/* ========================================================================== */

/*
 * HOW WIDE IS AN ELLIPSE AT A GIVEN ROW?
 *
 * Every circle/ellipse routine here is built on one question: at `a` rows
 * above (or below) the centre, how far does the shape reach left and right?
 * Answer that for every row and you can fill, outline, or thicken the shape
 * by drawing spans.
 *
 * A point (x, y) is inside an ellipse with radii rx, ry when
 *
 *        x^2       y^2
 *      ------  +  ------  <=  1
 *       rx^2       ry^2
 *
 * We use radii of (rx + 0.5) and (ry + 0.5), so a pixel counts as inside when
 * the curve passes through the middle of it. That's the same rule the
 * well-known "midpoint circle" algorithm uses, and it makes a circle of radius
 * r exactly 2r+1 pixels across. Multiplying everything by 4 (A = 2rx+1,
 * B = 2ry+1) keeps it in whole numbers:
 *
 *      x^2 <= A^2 * (B^2 - 4a^2) / (4 B^2)
 *
 * so the half-width is the integer square root of the right-hand side.
 * 64-bit maths keeps this exact for any radius a screen could show.
 *
 * Returns -1 if row `a` is outside the ellipse.
 */
static int32_t ellipse_half_width(int32_t rx, int32_t ry, int32_t a)
{
    if (rx < 0 || ry < 0 || a > ry) {
        return -1;
    }
    int64_t A   = 2 * (int64_t)rx + 1;
    int64_t B   = 2 * (int64_t)ry + 1;
    int64_t num = A * A * (B * B - 4 * (int64_t)a * a);
    int64_t den = 4 * B * B;
    return (int32_t)isqrt64((uint64_t)(num / den));
}

/*
 * ARC WINDOWS: WHICH PIXELS ARE INSIDE THE ANGLE RANGE?
 *
 * The obvious test is to compute each pixel's angle with atan2() and compare
 * it with the start and end angles. It works, but atan2() is by far the most
 * expensive operation in this library, and an arc outline has thousands of
 * pixels. (An earlier version did exactly that; timing it on the hardware
 * showed arcs 16 times slower than everything else, so it was replaced.)
 *
 * THE CROSS-PRODUCT TEST: no angles at all
 * Take a unit arrow S pointing at the start angle and E at the end angle,
 * worked out ONCE per arc. For a pixel at P (relative to the centre), the
 * 2-D cross product
 *
 *      cross(S, P) = S.x * P.y - S.y * P.x
 *
 * is positive when P lies counter-clockwise of S (within half a turn) and
 * negative when it lies clockwise. That's one multiply-subtract pair per
 * test, with no trigonometry.
 *
 *   Arc of half a turn or less:  P is inside when it's counter-clockwise of S
 *                                AND clockwise of E.
 *   Arc of more than half a turn: it's easier to test the gap instead. P is
 *                                inside unless it's strictly inside the gap
 *                                between E and S, i.e. inside when it's
 *                                counter-clockwise of S OR clockwise of E.
 *
 *            E                          S = start, E = end, arc runs
 *             \     inside              counter-clockwise from S to E
 *              \   (both tests)
 *               \______ S
 *
 * The arrows are stored as whole numbers scaled by 4096, so the tests are
 * pure integer maths.
 *
 * SKIPPING ROWS
 * A 10-degree slice near the top of a dial only touches a handful of rows,
 * so we also work out the range of heights the arc can reach and the engine
 * skips every other row without looking at it.
 */
#define ARC_SCALE 4096.0f

typedef struct {
    bool    full;          /* whole outline: no angle test at all          */
    bool    wide;          /* sweep > 180 degrees: use the OR form          */
    int32_t sx, sy;        /* start arrow x4096 (y is UP here, as in maths) */
    int32_t ex, ey;        /* end arrow x4096                               */
    int32_t y_lo, y_hi;    /* heights the arc can reach, y up from centre   */
} arc_window_t;

/** Is the pixel at offset (dx, dy) from the centre inside the arc? */
static inline bool arc_contains(const arc_window_t *arc, int32_t dx, int32_t dy)
{
    if (arc->full) {
        return true;
    }
    /* Screen y grows DOWNWARD, but the arc's arrows use "up" as positive
     * (as in maths and QuickBasic), so flip the sign of dy.                  */
    int32_t px = dx, py = -dy;
    int32_t cs = arc->sx * py - arc->sy * px;   /* >= 0: P is CCW of start  */
    int32_t ce = px * arc->ey - py * arc->ex;   /* >= 0: P is CW of end     */
    return arc->wide ? (cs >= 0 || ce >= 0) : (cs >= 0 && ce >= 0);
}

/** Draw a span of an arc's outline, keeping only pixels inside the angles.
 *  Accepted neighbours are grouped back into spans before sending.        */
static void arc_span(qg_screen_t *scr, int32_t cx, int32_t cy, int32_t xa,
                     int32_t xb, int32_t y, const arc_window_t *arc, qg_color_t c)
{
    int32_t run_start = 0;
    bool    in_run    = false;

    for (int32_t x = xa; x <= xb; x++) {
        bool inside = arc_contains(arc, x - cx, y - cy);
        if (inside && !in_run) {
            run_start = x;
            in_run    = true;
        } else if (!inside && in_run) {
            span(scr, run_start, x - 1, y, c);
            in_run = false;
        }
    }
    if (in_run) {
        span(scr, run_start, xb, y, c);
    }
}

/*
 * THE SHARED ELLIPSE ENGINE (used by circle, ellipse and arc)
 *
 * For each row `a` (0 at the centre, ry at the top/bottom) it works out:
 *
 *   outer  - the ellipse's half-width on this row
 *   ring   - where the outline starts, counting inward from the edge
 *
 * and draws the row as (stroke | fill | stroke):
 *
 *       -outer   -ring      ring    outer
 *          |######|  fill   |######|
 *
 * WHERE THE OUTLINE STARTS (1-pixel outline)
 *   An edge pixel is one that has an outside neighbour. On the far right
 *   that's always the last pixel (x = outer). Also, any pixel wider than the
 *   row beyond it (the next row out, a + 1) has nothing outside above or
 *   below it, so it's an edge too. So the outline runs from
 *   min(outer_next + 1, outer) to outer. Near the top and bottom that's a
 *   wide flat run; at the sides it's a single pixel. This keeps the outline
 *   gap-free and exactly on the edge of the fill.
 *
 * THICKER OUTLINES (line width t)
 *   The outline also includes everything outside a smaller ellipse with
 *   radii (rx - t, ry - t). Rows beyond that inner ellipse are all outline.
 *
 * Each row is drawn once, in its final colours, so nothing is sent twice.
 */
static void ellipse_engine(qg_screen_t *scr, int32_t cx, int32_t cy,
                           int32_t rx, int32_t ry, int32_t t,
                           qg_color_t stroke, qg_color_t fill,
                           const arc_window_t *arc)
{
    bool has_stroke = visible(stroke);
    bool has_fill   = visible(fill);
    if (rx < 0 || ry < 0 || (!has_stroke && !has_fill)) {
        return;
    }

    /* For arcs, only visit the rows the arc can actually reach. */
    int32_t a_first = 0, a_last = ry;
    if (arc != NULL && !arc->full) {
        int32_t lo = arc->y_lo, hi = arc->y_hi;          /* y up from centre */
        a_first = (lo <= 0 && hi >= 0) ? 0 : ((abs(lo) < abs(hi)) ? abs(lo) : abs(hi));
        a_last  = (abs(lo) > abs(hi)) ? abs(lo) : abs(hi);
        if (a_last > ry) a_last = ry;
    }

    for (int32_t a = a_first; a <= a_last; a++) {
        /* Rows are drawn in pairs, cy - a (upper) and cy + a (lower), moving
         * outward from the centre as `a` grows.
         *   - Once the upper row is above the screen AND the lower row is
         *     below it, every later pair is further out still: stop.
         *   - If both rows of this pair are off-screen but that isn't yet
         *     true, skip just this pair. (A centre below the screen, for
         *     example, starts off-screen and works its way up into view.)   */
        int32_t y_up = cy - a, y_down = cy + a;
        int32_t vt = qg_int_top(scr), vb = qg_int_bottom(scr);   /* the view */
        if (y_up < vt && y_down >= vb) break;
        bool up_off   = (y_up   < vt || y_up   >= vb);
        bool down_off = (y_down < vt || y_down >= vb);
        if (up_off && down_off) continue;

        int32_t outer = ellipse_half_width(rx, ry, a);
        if (outer < 0) continue;

        int32_t outer_next = ellipse_half_width(rx, ry, a + 1);   /* -1 past the top */
        int32_t ring = (outer_next + 1 < outer) ? outer_next + 1 : outer;

        if (t > 1) {
            int32_t irx = rx - t, iry = ry - t;
            int32_t inner = (irx >= 0 && iry >= 0) ? ellipse_half_width(irx, iry, a) : -1;
            if (inner + 1 < ring) ring = inner + 1;
        }

        /* Draw this row above and below the centre (once if a == 0). */
        for (int side = 0; side < 2; side++) {
            if (side == 1 && a == 0) break;
            int32_t y = (side == 0) ? cy - a : cy + a;
            if (y < qg_int_top(scr) || y >= qg_int_bottom(scr)) continue;

            /* Arcs: skip this row if it's outside the arc's height range.
             * The upper row is a rows ABOVE the centre (+a in "y up" terms),
             * the lower row a rows below it (-a).                           */
            if (arc != NULL && !arc->full) {
                int32_t y_up = (side == 0) ? a : -a;
                if (y_up < arc->y_lo || y_up > arc->y_hi) continue;
            }

            if (has_stroke) {
                if (arc != NULL) {
                    if (ring == 0) {
                        arc_span(scr, cx, cy, cx - outer, cx + outer, y, arc, stroke);
                    } else {
                        arc_span(scr, cx, cy, cx - outer, cx - ring, y, arc, stroke);
                        arc_span(scr, cx, cy, cx + ring,  cx + outer, y, arc, stroke);
                    }
                } else if (ring == 0) {
                    span(scr, cx - outer, cx + outer, y, stroke);
                } else {
                    span(scr, cx - outer, cx - ring, y, stroke);
                    span(scr, cx + ring,  cx + outer, y, stroke);
                }
            }

            if (has_fill) {
                if (!has_stroke) {
                    span(scr, cx - outer, cx + outer, y, fill);
                } else if (ring >= 1) {
                    span(scr, cx - ring + 1, cx + ring - 1, y, fill);
                }
            }
        }
    }
}

void qg_ellipse(qg_screen_t *scr, int16_t cx, int16_t cy, int16_t rx, int16_t ry,
                 qg_color_t stroke, qg_color_t fill)
{
    if (scr == NULL || !scr->ready) return;
    ellipse_engine(scr, cx, cy, rx, ry, scr->line_width,
                   fg_or(scr, stroke), fg_or(scr, fill), NULL);
}

void qg_circle(qg_screen_t *scr, int16_t cx, int16_t cy, int16_t r,
                qg_color_t stroke, qg_color_t fill)
{
    qg_ellipse(scr, cx, cy, r, r, stroke, fill);
}

void qg_arc(qg_screen_t *scr, int16_t cx, int16_t cy, int16_t rx, int16_t ry,
             int16_t start_deg, int16_t end_deg, qg_color_t color)
{
    if (scr == NULL || !scr->ready) return;
    color = fg_or(scr, color);
    if (!visible(color)) return;

    /* Bring both angles into 0..359, then measure the counter-clockwise
     * sweep from start to end. Equal angles mean a full turn.               */
    int32_t s = ((start_deg % 360) + 360) % 360;
    int32_t e = ((end_deg   % 360) + 360) % 360;
    int32_t sweep = e - s;
    if (sweep <= 0) sweep += 360;

    arc_window_t arc = { .full = (sweep >= 360), .wide = (sweep > 180) };

    if (!arc.full) {
        const float to_rad = 3.14159265f / 180.0f;
        float ss = sinf((float)s * to_rad), sc = cosf((float)s * to_rad);
        float es = sinf((float)e * to_rad), ec = cosf((float)e * to_rad);

        /* Unit arrows towards the start and end angles, x4096, rounded. */
        arc.sx = (int32_t)lroundf(sc * ARC_SCALE);
        arc.sy = (int32_t)lroundf(ss * ARC_SCALE);
        arc.ex = (int32_t)lroundf(ec * ARC_SCALE);
        arc.ey = (int32_t)lroundf(es * ARC_SCALE);

        /* HEIGHT RANGE. A pixel at angle A and distance d from the centre
         * sits at height d * sin(A). Over the arc's angles, sin(A) ranges
         * between its values at the two ends, unless the arc passes straight
         * up (90 degrees, sin = +1) or straight down (270, sin = -1).
         * Outline pixels lie between the inner and outer edges, so the
         * distance d is between r_in and r_out. We allow a pixel of margin
         * on every side, since this only has to be a safe bound.            */
        float s_min = (ss < es) ? ss : es;
        float s_max = (ss > es) ? ss : es;
        if ((90  - s + 360) % 360 <= sweep) s_max =  1.0f;
        if ((270 - s + 360) % 360 <= sweep) s_min = -1.0f;

        int32_t t     = scr->line_width;
        float   r_out = (float)((rx > ry) ? rx : ry) + 1.0f;
        float   r_in  = (float)((rx < ry) ? rx : ry) - (float)t - 1.0f;
        if (r_in < 0.0f) r_in = 0.0f;

        float hi = (s_max >= 0.0f) ? r_out * s_max : r_in * s_max;
        float lo = (s_min <= 0.0f) ? r_out * s_min : r_in * s_min;
        arc.y_hi = (int32_t)ceilf(hi) + 1;
        arc.y_lo = (int32_t)floorf(lo) - 1;
    }

    ellipse_engine(scr, cx, cy, rx, ry, scr->line_width, color, QG_TRANSPARENT, &arc);
}

/* ========================================================================== */
/*  POINT and PAINT (framebuffer screens only)                                */
/* ========================================================================== */

qg_color_t qg_point(qg_screen_t *scr, int16_t x, int16_t y)
{
    if (scr == NULL || !scr->ready || scr->backend->get_pixel == NULL) return QG_NONE;
    int32_t sx = x + scr->origin_x, sy = y + scr->origin_y;       /* screen pixels */
    if (sx < scr->view_x0 || sy < scr->view_y0 ||
        sx >= scr->view_x1 || sy >= scr->view_y1)                  return QG_NONE;
    return scr->backend->get_pixel(scr, (int16_t)sx, (int16_t)sy);
}

/*
 * SCANLINE FLOOD FILL
 *
 * The simple way to flood-fill ("fill this pixel, then do the same for its
 * four neighbours") remembers every pixel it still has to visit, which can
 * mean tens of thousands of them. The scanline method works in whole
 * horizontal runs instead:
 *
 *   1. Take a seed point from the to-do list.
 *   2. Walk left and right from it to find the whole run of fillable pixels
 *      on that row, and fill it in one go.
 *   3. Look along the rows just above and below that run. Each separate
 *      stretch of fillable pixels there gets ONE new seed on the to-do list.
 *   4. Repeat until the list is empty.
 *
 *       ........#####.......          the run on this row is filled at
 *       ....#########.......    <--   once; the row above has one fillable
 *       ...############.....          stretch, so it gets one seed
 *
 * The to-do list only holds one entry per stretch still waiting, so a few
 * hundred entries cover any ordinary shape.
 */
typedef struct { int16_t x, y; } seed_t;
static seed_t s_seeds[QG_PAINT_STACK];

qg_err_t qg_paint(qg_screen_t *scr, int16_t x, int16_t y,
                    qg_color_t fill, qg_color_t border)
{
    if (scr == NULL || !scr->ready)             return QG_ERR_ARG;
    if (scr->fb == NULL)                        return QG_ERR_UNSUPPORTED;
    x = (int16_t)(x + scr->origin_x);                     /* to screen pixels */
    y = (int16_t)(y + scr->origin_y);
    if (x < scr->view_x0 || y < scr->view_y0 ||
        x >= scr->view_x1 || y >= scr->view_y1) return QG_ERR_ARG;
    fill = fg_or(scr, fill);
    if (!visible(fill))                         return QG_OK;

    /* The fill stays inside the view: its edges act like a border. */
    const int32_t  w  = scr->width;
    const int32_t  vx0 = scr->view_x0, vx1 = scr->view_x1;   /* exclusive */
    const int32_t  vy0 = scr->view_y0, vy1 = scr->view_y1;
    uint8_t *const fb = scr->fb;
    const bool     bucket = !visible(border);          /* QG_DEFAULT etc. */
    const uint8_t  seed_col = fb[(uint32_t)y * (uint32_t)w + (uint32_t)x];
    const uint8_t  f = (uint8_t)fill, b = (uint8_t)border;

    /* Which pixels may be filled?
     *   bucket mode: those still the starting colour
     *   border mode: anything that's neither the border nor already filled */
    #define FILLABLE(px) (bucket ? ((px) == seed_col) : ((px) != b && (px) != f))

    if (bucket && seed_col == f) return QG_OK;         /* already that colour */

    int32_t   top = 0;
    qg_err_t result = QG_OK;
    int32_t   minx = x, maxx = x, miny = y, maxy = y;   /* for the flush */

    s_seeds[top++] = (seed_t){ x, y };
    while (top > 0) {
        seed_t   sd  = s_seeds[--top];
        uint8_t *row = fb + (uint32_t)sd.y * (uint32_t)w;
        if (!FILLABLE(row[sd.x])) continue;             /* filled meanwhile */

        /* 2. Find and fill the run. */
        int32_t l = sd.x, r = sd.x;
        while (l > vx0     && FILLABLE(row[l - 1])) l--;
        while (r < vx1 - 1 && FILLABLE(row[r + 1])) r++;
        memset(row + l, f, (size_t)(r - l + 1));
        if (l < minx) minx = l;
        if (r > maxx) maxx = r;
        if (sd.y < miny) miny = sd.y;
        if (sd.y > maxy) maxy = sd.y;

        /* 3. One seed per fillable stretch above and below. */
        for (int dir = -1; dir <= 1; dir += 2) {
            int32_t ny = sd.y + dir;
            if (ny < vy0 || ny >= vy1) continue;
            const uint8_t *nrow = fb + (uint32_t)ny * (uint32_t)w;
            bool in_stretch = false;
            for (int32_t i = l; i <= r; i++) {
                bool ok = FILLABLE(nrow[i]);
                if (ok && !in_stretch) {
                    if (top == QG_PAINT_STACK) { result = QG_ERR_OVERFLOW; break; }
                    s_seeds[top++] = (seed_t){ (int16_t)i, (int16_t)ny };
                }
                in_stretch = ok;
            }
        }
    }
    #undef FILLABLE

    qg_int_dirty(scr, minx, miny, maxx - minx + 1, maxy - miny + 1);
    return result;
}
