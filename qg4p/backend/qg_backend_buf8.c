/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    qg_backend_buf8.c
 * @brief   BUF8 backend: drawing goes into an 8-bit framebuffer in RAM, and
 *          qg_screen_flush() sends it to the panel.
 *
 * LAYER:   Surface backend
 * DEPENDS: qg_screen.h, qg_internal.h, drivers/qg_driver.h, hal/qg_hal.h
 *
 * ---------------------------------------------------------------------------
 *  WHY A FRAMEBUFFER?
 * ---------------------------------------------------------------------------
 *  On a DIRECT screen, every drawing step appears on the glass immediately.
 *  Animating something means erasing it and redrawing it, and for an instant
 *  the viewer sees the gap: flicker. With a framebuffer, all the steps
 *  happen in RAM, invisibly, and qg_screen_flush() sends the finished
 *  picture. The viewer only ever sees complete frames.
 *
 *  Each pixel is ONE byte: a palette index, not a colour. That halves the
 *  memory (a 320x480 screen needs 150 KB instead of 300 KB), and it means
 *  changing a palette entry recolours every pixel that uses it, for free,
 *  at the next flush. That's "palette animation".
 *
 * ---------------------------------------------------------------------------
 *  CHANGED-AREA TRACKING
 * ---------------------------------------------------------------------------
 *  Every write stretches a rectangle (dirty_x0..x1, dirty_y0..y1) to cover
 *  it. The flush sends only that rectangle, then forgets it. Moving one small
 *  sprite therefore costs a small transfer, not a whole screen.
 *
 * ---------------------------------------------------------------------------
 *  THE FLUSH, OVERLAPPED
 * ---------------------------------------------------------------------------
 *  The panel wants RGB565; the framebuffer holds indices. So the flush
 *  converts, a chunk at a time, into one of two buffers, and while DMA sends
 *  one buffer the CPU fills the other:
 *
 *      CPU:  convert A | convert B | convert A | convert B | ...
 *      DMA:            |  send A   |  send B   |  send A   | ...
 *
 *  The conversion (one table lookup per pixel) is quicker than the transfer,
 *  so it hides almost entirely behind it.
 */
#include <string.h>
#include "qg_screen.h"
#include "qg_internal.h"
#include "drivers/qg_driver.h"
#include "hal/qg_hal.h"

static uint16_t s_chunk[2][QG_BUF8_CHUNK_PIXELS];

static void buf8_fill_rect(qg_screen_t *scr, int16_t x, int16_t y,
                           int16_t w, int16_t h, qg_color_t color)
{
    uint8_t *row = scr->fb + (uint32_t)y * (uint32_t)scr->width + (uint32_t)x;
    for (int16_t j = 0; j < h; j++, row += scr->width) {
        memset(row, (int)color, (size_t)w);
    }
    qg_int_dirty(scr, x, y, w, h);
}

static void buf8_put_idx_row(qg_screen_t *scr, int16_t x, int16_t y, int16_t w,
                             const uint8_t *idx, bool skip_255)
{
    uint8_t *row = scr->fb + (uint32_t)y * (uint32_t)scr->width + (uint32_t)x;
    if (skip_255) {
        for (int16_t i = 0; i < w; i++) {
            if (idx[i] != 255) row[i] = idx[i];
        }
    } else {
        memcpy(row, idx, (size_t)w);
    }
    qg_int_dirty(scr, x, y, w, 1);
}

static uint8_t buf8_get_pixel(qg_screen_t *scr, int16_t x, int16_t y)
{
    return scr->fb[(uint32_t)y * (uint32_t)scr->width + (uint32_t)x];
}

/* Scrolling is just moving bytes: the whole screen shifts up in one
 * memmove(), no reprinting needed.                                          */
static void buf8_scroll_up(qg_screen_t *scr, int16_t dy, qg_color_t bg)
{
    uint32_t w = (uint32_t)scr->width, h = (uint32_t)scr->height;
    if (dy <= 0) return;
    if ((uint32_t)dy >= h) {
        memset(scr->fb, (int)bg, w * h);
    } else {
        memmove(scr->fb, scr->fb + (uint32_t)dy * w, (h - (uint32_t)dy) * w);
        memset(scr->fb + (h - (uint32_t)dy) * w, (int)bg, (uint32_t)dy * w);
    }
    qg_int_dirty_all(scr);
}

static void buf8_flush(qg_screen_t *scr)
{
    if (!scr->dirty) return;

    const int32_t x0 = scr->dirty_x0, y0 = scr->dirty_y0;
    const int32_t w  = scr->dirty_x1 - x0 + 1, h = scr->dirty_y1 - y0 + 1;
    const uint16_t *pal = qg_int_out(scr);           /* the palette as sent */

    qg_hal_begin(&scr->dev);
    qg_driver_set_window(&scr->dev,
                          (uint16_t)(x0 + scr->ram_x_off), (uint16_t)(y0 + scr->ram_y_off),
                          (uint16_t)(x0 + w - 1 + scr->ram_x_off),
                          (uint16_t)(y0 + h - 1 + scr->ram_y_off));
    qg_hal_stream_begin(&scr->dev);

    int      buf = 0;
    uint32_t n   = 0;
    for (int32_t y = y0; y < y0 + h; y++) {
        const uint8_t *src = scr->fb + (uint32_t)y * (uint32_t)scr->width + (uint32_t)x0;
        for (int32_t x = 0; x < w; x++) {
            s_chunk[buf][n++] = pal[src[x]];
            if (n == QG_BUF8_CHUNK_PIXELS) {
                qg_hal_stream_pixels(&scr->dev, s_chunk[buf], n);  /* starts sending */
                buf ^= 1;                                           /* fill the other */
                n = 0;
            }
        }
    }
    if (n > 0) {
        qg_hal_stream_pixels(&scr->dev, s_chunk[buf], n);
    }
    qg_hal_stream_end(&scr->dev);
    qg_hal_end(&scr->dev);

    scr->dirty = false;
}

/* Colour-match tables for images drawn on framebuffer screens. They live
 * here, not in qg_image.c, so only programs with framebuffer screens carry
 * them (about 1 KB).                                                        */
static qg_remap_cache_t s_remap;

const qg_backend_t qg_backend_buf8 = {
    .needs_framebuffer = true,
    .image_cache  = &s_remap,
    .name         = "BUF8",
    .fill_rect    = buf8_fill_rect,
    .write_rgb565 = NULL,             /* indices, not colours: see header */
    .put_idx_row  = buf8_put_idx_row,
    .get_pixel    = buf8_get_pixel,
    .scroll_up    = buf8_scroll_up,
    .flush        = buf8_flush,
};
