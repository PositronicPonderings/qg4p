/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    qg_internal.h
 * @brief   Library-internal helpers. NOT part of the public API.
 *
 * LAYER:   Internal (shared by the library's own .c files)
 * DEPENDS: qg_screen.h
 *
 * Functions here use the prefix qg_int_ to mark them as internal. They may
 * change between versions. Application code should use only what qg4p.h
 * provides. (The first two demos borrow qg_int_fill_rect() because they
 * were written before qg_box() existed.)
 *
 * Sizes are 32-bit here even though screen coordinates fit in 16 bits, so
 * that intermediate maths (e.g. a huge shape mostly off-screen) can't
 * overflow before clipping trims it.
 */
#ifndef QG_INTERNAL_H
#define QG_INTERNAL_H

#include "qg_screen.h"

/**
 * Fill a rectangle given as top-left corner + width/height, with clipping.
 *
 * This is the single gateway between drawing code and the backend. It:
 *   1. ignores QG_TRANSPARENT (nothing to draw),
 *   2. resolves QG_DEFAULT to the screen's background colour,
 *   3. clips the rectangle to the screen, discarding anything off-screen,
 *   4. hands the safe, clipped rectangle to the backend.
 */
void qg_int_fill_rect(qg_screen_t *scr, int32_t x, int32_t y,
                       int32_t w, int32_t h, qg_color_t color);

/* --- The view, in the coordinates callers use ----------------------------
 * Callers pass coordinates relative to the origin (which qg_view() may have
 * moved). These give the view's edges in those same coordinates, so that
 * drawing code can skip rows or columns that can't be seen.              */
static inline int32_t qg_int_left(const qg_screen_t *s)   { return s->view_x0 - s->origin_x; }
static inline int32_t qg_int_right(const qg_screen_t *s)  { return s->view_x1 - s->origin_x; } /* exclusive */
static inline int32_t qg_int_top(const qg_screen_t *s)    { return s->view_y0 - s->origin_y; }
static inline int32_t qg_int_bottom(const qg_screen_t *s) { return s->view_y1 - s->origin_y; } /* exclusive */

/** True when no smaller view is set (the view is the whole screen). */
static inline bool qg_int_view_full(const qg_screen_t *s)
{
    return s->view_x0 == 0 && s->view_y0 == 0 &&
           s->view_x1 == s->width && s->view_y1 == s->height;
}

/* --- Colour matching for images on framebuffer screens ---------------------
 * Tables mapping an image's palette onto a screen's (see qg_image.c). They
 * belong to the framebuffer backend (its image_cache), so a program without
 * framebuffer screens doesn't carry them.                                  */
#define QG_REMAP_CACHE 4
typedef struct {
    const uint8_t      *img_palette;   /* identifies the image's palette   */
    const qg_screen_t  *scr;
    uint16_t            gen;           /* screen palette version           */
    uint8_t             map[256];
} qg_remap_t;
typedef struct {
    qg_remap_t tables[QG_REMAP_CACHE];
    uint8_t    next;
} qg_remap_cache_t;

/* --- The palette as sent -------------------------------------------------
 * Every place that converts a palette index into a colour for the PANEL uses
 * this, so a colour adjustment (qg_screen_set_color_adjust) applies to
 * everything. Without one, it's simply the palette.                         */
static inline const uint16_t *qg_int_out(const qg_screen_t *s)
{
    return s->adjust ? s->adjust->out : s->palette;
}

/** Adjust one 8-bit-per-channel colour and pack it as RGB565 (for images). */
static inline uint16_t qg_int_out_rgb(const qg_screen_t *s, uint8_t r, uint8_t g, uint8_t b)
{
    if (s->adjust) {
        r = s->adjust->lut[0][r]; g = s->adjust->lut[1][g]; b = s->adjust->lut[2][b];
    }
    return QG_RGB565(r, g, b);
}

/** Recompute the as-sent copy of palette entries [first, first + count). */
void qg_int_palette_sync(qg_screen_t *s, int first, int count);

/* --- BUF8: tracking what changed since the last flush ------------------- */

/** Add a (clipped) rectangle to the area the next flush must send. */
static inline void qg_int_dirty(qg_screen_t *scr, int32_t x, int32_t y, int32_t w, int32_t h)
{
    if (scr->fb == NULL || w <= 0 || h <= 0) return;
    int32_t x1 = x + w - 1, y1 = y + h - 1;
    if (!scr->dirty) {
        scr->dirty = true;
        scr->dirty_x0 = (int16_t)x;  scr->dirty_y0 = (int16_t)y;
        scr->dirty_x1 = (int16_t)x1; scr->dirty_y1 = (int16_t)y1;
        return;
    }
    if (x  < scr->dirty_x0) scr->dirty_x0 = (int16_t)x;
    if (y  < scr->dirty_y0) scr->dirty_y0 = (int16_t)y;
    if (x1 > scr->dirty_x1) scr->dirty_x1 = (int16_t)x1;
    if (y1 > scr->dirty_y1) scr->dirty_y1 = (int16_t)y1;
}

/** Mark the whole screen as changed (e.g. after a palette change). */
static inline void qg_int_dirty_all(qg_screen_t *scr)
{
    qg_int_dirty(scr, 0, 0, scr->width, scr->height);
}

#endif /* QG_INTERNAL_H */
