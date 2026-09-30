/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    qg_backend.h
 * @brief   The backend interface: where a screen's pixels actually go.
 *
 * LAYER:   Surface backend (between drawing code and panel drivers)
 * DEPENDS: qg_types.h
 *
 * ---------------------------------------------------------------------------
 *  WHY A BACKEND LAYER?
 * ---------------------------------------------------------------------------
 *  Drawing code (boxes, circles, text, images) should not care whether pixels
 *  go straight to the glass (DIRECT) or into a RAM framebuffer (BUF8). So
 *  every screen carries a pointer to a small table of functions, its
 *  "backend", and all drawing goes through it:
 *
 *      qg_cls()  ->  scr->backend->fill_rect(...)
 *                         |
 *               +---------+----------+
 *               |                    |
 *        DIRECT backend        BUF8 backend
 *        send to panel         write into RAM
 *
 *  This is C's version of an "interface": a struct of function pointers.
 *  Adding BUF8 later means writing one new file that fills in this struct,
 *  with no changes to the drawing code.
 *
 * ---------------------------------------------------------------------------
 *  CONTRACT
 * ---------------------------------------------------------------------------
 *  - Coordinates are ALREADY CLIPPED to the screen by the caller.
 *    Backends may assume 0 <= x, x+w <= width, and so on. Clipping lives in
 *    exactly one place (qg_draw.c) so it cannot be forgotten.
 *  - Colours are real palette indices 0..254. QG_TRANSPARENT and QG_DEFAULT
 *    have been resolved by the caller.
 *
 *  Every shape is drawn through fill_rect: a horizontal span is simply a
 *  rectangle one pixel tall, and a pixel is a 1x1 rectangle. The HAL sends
 *  short runs without DMA, so small rectangles stay cheap. write_rgb565
 *  sends a whole block of pixels in one go (opaque text and images).
 *
 *  A backend can leave an operation NULL when it doesn't apply. BUF8 has no
 *  write_rgb565 (its memory holds palette indices, not RGB565), so opaque
 *  text and images use its index operations instead; DIRECT has none of the
 *  framebuffer-only operations, so qg_point/qg_paint report "unsupported".
 */
#ifndef QG_BACKEND_H
#define QG_BACKEND_H

#include "qg_types.h"

typedef struct qg_screen qg_screen_t;   /* defined in qg_screen.h */

typedef struct qg_backend {
    const char *name;
    bool        needs_framebuffer;  /**< Does the config have to supply one? */

    /** Backend-private storage the image code may use (the framebuffer
     *  backend's colour-match cache); NULL if none.                        */
    void       *image_cache;

    /** Fill a clipped rectangle with one palette colour. */
    void (*fill_rect)(qg_screen_t *scr, int16_t x, int16_t y,
                      int16_t w, int16_t h, qg_color_t color);

    /**
     * Send a block of ready-made RGB565 pixels (row by row, left to right).
     * The rectangle must be entirely on-screen: the caller checks. Used for
     * opaque text and images.
     */
    void (*write_rgb565)(qg_screen_t *scr, int16_t x, int16_t y,
                         int16_t w, int16_t h, const uint16_t *pixels);

    /* --- Framebuffer-only operations (NULL on DIRECT) ------------------- */

    /** Store one clipped row of palette indices; skip 255s if `skip_255`. */
    void (*put_idx_row)(qg_screen_t *scr, int16_t x, int16_t y, int16_t w,
                        const uint8_t *idx, bool skip_255);

    /** Read one pixel's palette index (x, y already on-screen). */
    uint8_t (*get_pixel)(qg_screen_t *scr, int16_t x, int16_t y);

    /** Move everything up by dy rows; the rows uncovered get colour `bg`. */
    void (*scroll_up)(qg_screen_t *scr, int16_t dy, qg_color_t bg);

    /** Send what changed since the last flush to the panel. */
    void (*flush)(qg_screen_t *scr);
} qg_backend_t;

/** Pixels go straight to the panel over SPI. */
extern const qg_backend_t qg_backend_direct;

/** Pixels go into an 8-bit framebuffer in RAM; qg_screen_flush() shows them. */
extern const qg_backend_t qg_backend_buf8;

#endif /* QG_BACKEND_H */
