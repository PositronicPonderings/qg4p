/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    qg_backend_direct.c
 * @brief   DIRECT backend: every drawing operation goes straight to the panel.
 *
 * LAYER:   Surface backend
 * DEPENDS: qg_screen.h, drivers/qg_driver.h, hal/qg_hal.h
 *
 * HOW A FILLED RECTANGLE REACHES THE GLASS
 *   1. Take the bus (CS low).
 *   2. Set the chip's "window" to the rectangle. Panel offsets are added here,
 *      the one place where screen coordinates become chip-RAM coordinates.
 *   3. Look up the palette colour -> RGB565.
 *   4. DMA the same 16-bit value w*h times; the chip wraps rows by itself.
 *   5. Release the bus (CS high).
 *
 *  A 240x320 clear is 76,800 pixels, or 153,600 bytes. At 37.5 MHz that takes
 *  about 33 ms and needs zero RAM.
 */
#include "qg_screen.h"
#include "qg_internal.h"
#include "drivers/qg_driver.h"
#include "hal/qg_hal.h"

static void direct_fill_rect(qg_screen_t *scr, int16_t x, int16_t y,
                             int16_t w, int16_t h, qg_color_t color)
{
    /* Convert screen coordinates to chip RAM coordinates. */
    uint16_t x0 = (uint16_t)(x + scr->ram_x_off);
    uint16_t y0 = (uint16_t)(y + scr->ram_y_off);
    uint16_t x1 = (uint16_t)(x0 + w - 1);
    uint16_t y1 = (uint16_t)(y0 + h - 1);

    qg_hal_begin(&scr->dev);
    qg_driver_set_window(&scr->dev, x0, y0, x1, y1);
    qg_hal_fill_pixels(&scr->dev, qg_int_out(scr)[color & 0xFFu],
                        (uint32_t)w * (uint32_t)h);
    qg_hal_end(&scr->dev);
}

static void direct_write_rgb565(qg_screen_t *scr, int16_t x, int16_t y,
                                int16_t w, int16_t h, const uint16_t *pixels)
{
    uint16_t x0 = (uint16_t)(x + scr->ram_x_off);
    uint16_t y0 = (uint16_t)(y + scr->ram_y_off);

    qg_hal_begin(&scr->dev);
    qg_driver_set_window(&scr->dev, x0, y0,
                          (uint16_t)(x0 + w - 1), (uint16_t)(y0 + h - 1));
    qg_hal_write_pixels(&scr->dev, pixels, (uint32_t)w * (uint32_t)h);
    qg_hal_end(&scr->dev);
}

const qg_backend_t qg_backend_direct = {
    .needs_framebuffer = false,
    .name         = "DIRECT",
    .fill_rect    = direct_fill_rect,
    .write_rgb565 = direct_write_rgb565,
};
