/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    qg_block.c
 * @brief   GET and PUT.
 *
 * LAYER:   Public API (drawing)
 * DEPENDS: qg_block.h, qg_internal.h
 *
 * On a framebuffer screen both work directly on the bytes in RAM, which is
 * what makes the bitwise modes possible: they need the pixel that's already
 * there. On a DIRECT screen, PUT converts each row to RGB565 through the
 * palette and sends it, row by row (or run by run, for TRANSPARENT).
 */
#include "qg_block.h"
#include "qg_internal.h"

qg_err_t qg_get(qg_screen_t *scr, int16_t x1, int16_t y1, int16_t x2, int16_t y2,
                uint8_t *buf, uint32_t buf_size)
{
    if (scr == NULL || !scr->ready || buf == NULL) return QG_ERR_ARG;
    if (scr->fb == NULL)                           return QG_ERR_UNSUPPORTED;

    /* Corners in any order, then to screen pixels. */
    int32_t l = ((x1 < x2) ? x1 : x2) + scr->origin_x, r = ((x1 < x2) ? x2 : x1) + scr->origin_x;
    int32_t t = ((y1 < y2) ? y1 : y2) + scr->origin_y, b = ((y1 < y2) ? y2 : y1) + scr->origin_y;
    if (l < scr->view_x0 || t < scr->view_y0 || r >= scr->view_x1 || b >= scr->view_y1) {
        return QG_ERR_ARG;                      /* must be entirely in the view */
    }
    int32_t w = r - l + 1, h = b - t + 1;
    if (buf_size < QG_BLOCK_BYTES(w, h)) return QG_ERR_ARG;

    buf[0] = (uint8_t)w; buf[1] = (uint8_t)(w >> 8);      /* header: little-endian */
    buf[2] = (uint8_t)h; buf[3] = (uint8_t)(h >> 8);
    uint8_t *dst = buf + 4;
    for (int32_t y = t; y <= b; y++) {
        const uint8_t *src = scr->fb + (uint32_t)y * (uint32_t)scr->width + (uint32_t)l;
        for (int32_t i = 0; i < w; i++) *dst++ = src[i];
    }
    return QG_OK;
}

static uint16_t s_row[QG_IMAGE_MAX_WIDTH];     /* DIRECT: one row as RGB565 */

qg_err_t qg_put(qg_screen_t *scr, int16_t x, int16_t y, const uint8_t *buf, qg_put_t mode)
{
    if (scr == NULL || !scr->ready || buf == NULL) return QG_ERR_ARG;
    const bool buffered = (scr->fb != NULL);
    const bool bitwise  = (mode == QG_PUT_AND || mode == QG_PUT_OR || mode == QG_PUT_XOR);
    if (bitwise && !buffered)                      return QG_ERR_UNSUPPORTED;

    const int32_t bw = qg_block_width(buf), bh = qg_block_height(buf);
    const uint8_t *px = buf + 4;

    /* To screen pixels, then clip to the view. (c0, r0) is the first block
     * column and row that survive; (sx, sy) where they land on the screen.  */
    int32_t sx = x + scr->origin_x, sy = y + scr->origin_y;
    int32_t c0 = 0, r0 = 0, cw = bw, rh = bh;
    if (sx < scr->view_x0) { c0 = scr->view_x0 - sx; cw -= c0; sx = scr->view_x0; }
    if (sy < scr->view_y0) { r0 = scr->view_y0 - sy; rh -= r0; sy = scr->view_y0; }
    if (sx + cw > scr->view_x1) cw = scr->view_x1 - sx;
    if (sy + rh > scr->view_y1) rh = scr->view_y1 - sy;
    if (cw <= 0 || rh <= 0) return QG_OK;             /* nothing visible */

    for (int32_t j = 0; j < rh; j++) {
        const uint8_t *src = px + (uint32_t)(r0 + j) * (uint32_t)bw + (uint32_t)c0;

        if (buffered) {
            /* Framebuffer: combine with what's there, byte by byte. */
            uint8_t *d = scr->fb + (uint32_t)(sy + j) * (uint32_t)scr->width + (uint32_t)sx;
            for (int32_t i = 0; i < cw; i++) {
                uint8_t v = src[i];
                switch (mode) {
                case QG_PUT_PSET:        d[i] = v;                 break;
                case QG_PUT_PRESET:      d[i] = (uint8_t)(255 - v); break;
                case QG_PUT_AND:         d[i] &= v;                break;
                case QG_PUT_OR:          d[i] |= v;                break;
                case QG_PUT_XOR:         d[i] ^= v;                break;
                case QG_PUT_TRANSPARENT: if (v != 255) d[i] = v;   break;
                }
            }
            continue;
        }

        /* DIRECT: convert the visible row to RGB565 and send it. */
        int32_t n = (cw > QG_IMAGE_MAX_WIDTH) ? QG_IMAGE_MAX_WIDTH : cw;
        const uint16_t *out = qg_int_out(scr);         /* the palette as sent */
        for (int32_t i = 0; i < n; i++) {
            uint8_t v = (mode == QG_PUT_PRESET) ? (uint8_t)(255 - src[i]) : src[i];
            s_row[i] = out[v];
        }
        if (mode != QG_PUT_TRANSPARENT) {
            scr->backend->write_rgb565(scr, (int16_t)sx, (int16_t)(sy + j), (int16_t)n, 1, s_row);
        } else {
            for (int32_t i = 0; i < n; ) {                    /* solid runs only */
                while (i < n && src[i] == 255) i++;
                int32_t start = i;
                while (i < n && src[i] != 255) i++;
                if (i > start) {
                    scr->backend->write_rgb565(scr, (int16_t)(sx + start), (int16_t)(sy + j),
                                               (int16_t)(i - start), 1, &s_row[start]);
                }
            }
        }
    }
    if (buffered) qg_int_dirty(scr, sx, sy, cw, rh);
    return QG_OK;
}
