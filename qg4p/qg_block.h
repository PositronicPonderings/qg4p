/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    qg_block.h
 * @brief   GET and PUT: copy a rectangle of the screen into memory, and
 *          stamp it back anywhere, in one of six ways.
 *
 * LAYER:   Public API (drawing)
 * DEPENDS: qg_screen.h
 *
 * QuickBasic's sprite system, 1987 edition:
 *
 *     static uint8_t ship[QG_BLOCK_BYTES(32, 16)];      // room for 32x16
 *     qg_get(&scr, 0, 0, 31, 15, ship, sizeof ship);    // capture it
 *     qg_put(&scr, 100, 50, ship, QG_PUT_TRANSPARENT);  // stamp it
 *
 * A block is plain bytes: a 4-byte header (width, height) and then one
 * palette index per pixel, row by row. QG_BLOCK_BYTES() gives the size.
 *
 * WHICH SCREENS
 *   qg_get() reads pixels back, so it needs a framebuffer (BUF8) screen.
 *   qg_put() works on any screen in the PSET, PRESET and TRANSPARENT modes.
 *   The AND, OR and XOR modes combine the block with what's already there,
 *   so they too need a framebuffer screen.
 *
 * THE MODES (QuickBasic's five, plus one)
 *   QG_PUT_PSET         copy the block as it is
 *   QG_PUT_PRESET       copy it with every index inverted (255 - index)
 *   QG_PUT_AND          screen = screen AND block   (bitwise, on the indices)
 *   QG_PUT_OR           screen = screen OR block
 *   QG_PUT_XOR          screen = screen XOR block. Doing it twice restores
 *                       the screen exactly: the classic way to move a
 *                       sprite without saving what's underneath
 *   QG_PUT_TRANSPARENT  copy, but skip index 255 (see-through pixels)
 *
 *   The bitwise modes work on palette index NUMBERS, as QuickBasic's did on
 *   its colour numbers. With the standard palette the resulting colours look
 *   fairly random; arrange a palette for it (or just enjoy XOR's
 *   reversibility). Results can include index 255: see qg_palette_set().
 *
 * Unlike QuickBasic, which stopped with "Illegal function call", a PUT that
 * hangs off the screen (or the view) is simply cut off. That's what you
 * want for sprites sliding in from the edge.
 */
#ifndef QG_BLOCK_H
#define QG_BLOCK_H

#include "qg_screen.h"

/** Bytes needed to hold a w x h block. */
#define QG_BLOCK_BYTES(w, h) (4u + (uint32_t)(w) * (uint32_t)(h))

/** How qg_put() combines the block with the screen. */
typedef enum {
    QG_PUT_PSET = 0,
    QG_PUT_PRESET,
    QG_PUT_AND,
    QG_PUT_OR,
    QG_PUT_XOR,
    QG_PUT_TRANSPARENT
} qg_put_t;

/**
 * Copy a rectangle of the screen into `buf`. QuickBasic: GET (x1,y1)-(x2,y2)
 * Corners in any order, both included, relative to the view's origin.
 * @return QG_OK; QG_ERR_UNSUPPORTED on a DIRECT screen; QG_ERR_ARG if the
 *         rectangle isn't entirely inside the view, or buf is too small.
 */
qg_err_t qg_get(qg_screen_t *scr, int16_t x1, int16_t y1, int16_t x2, int16_t y2,
                uint8_t *buf, uint32_t buf_size);

/**
 * Stamp a block with its top-left corner at (x, y). QuickBasic: PUT (x,y), a, mode
 * @return QG_OK; QG_ERR_UNSUPPORTED for AND/OR/XOR on a DIRECT screen;
 *         QG_ERR_ARG for a missing block.
 */
qg_err_t qg_put(qg_screen_t *scr, int16_t x, int16_t y, const uint8_t *buf, qg_put_t mode);

/** A block's size, from its header. */
static inline int16_t qg_block_width(const uint8_t *buf)  { return (int16_t)(buf[0] | (buf[1] << 8)); }
static inline int16_t qg_block_height(const uint8_t *buf) { return (int16_t)(buf[2] | (buf[3] << 8)); }

#endif /* QG_BLOCK_H */
