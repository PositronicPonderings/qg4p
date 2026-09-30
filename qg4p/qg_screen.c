/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    qg_screen.c
 * @brief   Screen bring-up, rotation (including panel-offset maths),
 *          backlight on/off and brightness.
 *
 * LAYER:   Public API (screens)
 * DEPENDS: qg_screen.h, qg_palette.h, qg_draw.h, pico_stdlib
 */
#include <string.h>
#include "pico/stdlib.h"
#include "qg_screen.h"
#include "qg_palette.h"
#include "qg_draw.h"
#include "qg_internal.h"

/* ========================================================================== */
/*  Rotation                                                                  */
/* ========================================================================== */

qg_err_t qg_screen_set_rotation(qg_screen_t *scr, qg_rotation_t rot)
{
    if (scr == NULL || scr->drv == NULL) {
        return QG_ERR_ARG;
    }

    const qg_screen_config_t *c   = &scr->cfg;
    const qg_driver_t        *drv = scr->drv;

    uint8_t mad = qg_driver_madctl(rot, c->mirror_x, c->mirror_y, c->bgr);
    bool mx = (mad & QG_MADCTL_MX) != 0;
    bool my = (mad & QG_MADCTL_MY) != 0;
    bool mv = (mad & QG_MADCTL_MV) != 0;

    /*
     * PANEL OFFSETS UNDER ROTATION
     *
     * The chip's RAM can be bigger than the glass. Example: the 1.54" board's
     * glass is 240x240, but the ST7789's RAM is 240x320:
     *
     *        RAM (240 x 320)                  visible glass (240 x 240)
     *     +------------------+  row 0      +------------------+
     *     |                  |             |                  |
     *     |     visible      |             |                  |
     *     |     240x240      |             |                  |
     *     |                  |  row 239    +------------------+
     *     |------------------|
     *     |   unused  (80)   |  row 319
     *     +------------------+
     *
     * With no mirroring, the glass starts at row y_offset (0 here). Mirroring
     * rows (MY) makes the chip count rows from the other end, so the glass
     * now starts at  ram_h - height - y_offset = 320 - 240 - 0 = 80.
     * The same logic applies to columns with MX.
     *
     * Finally, MV swaps the axes: the screen's X then walks along the chip's
     * rows, so the two offsets trade places.
     *
     * Getting this wrong shows up as a blank or garbage strip along one edge.
     * The border in the demo's orientation test is there to catch it.
     */
    uint16_t col_off = mx ? (uint16_t)(drv->ram_w - c->width  - c->x_offset) : c->x_offset;
    uint16_t row_off = my ? (uint16_t)(drv->ram_h - c->height - c->y_offset) : c->y_offset;

    if (mv) {
        scr->width     = (int16_t)c->height;
        scr->height    = (int16_t)c->width;
        scr->ram_x_off = row_off;
        scr->ram_y_off = col_off;
    } else {
        scr->width     = (int16_t)c->width;
        scr->height    = (int16_t)c->height;
        scr->ram_x_off = col_off;
        scr->ram_y_off = row_off;
    }
    scr->rotation = rot;
    qg_view_reset(scr);         /* the old view no longer fits the new shape */

    qg_hal_begin(&scr->dev);
    qg_hal_write_cmd(&scr->dev, QG_CMD_MADCTL);
    qg_hal_write_data(&scr->dev, &mad, 1);
    qg_hal_end(&scr->dev);

    qg_int_dirty_all(scr);     /* BUF8: the next flush sends everything */
    return QG_OK;
}

/* ========================================================================== */
/*  Backlight                                                                 */
/* ========================================================================== */

void qg_screen_backlight(qg_screen_t *scr, bool on)
{
    if (scr == NULL) {
        return;
    }
    scr->bl_on = on;
    qg_hal_backlight_set(scr->cfg.bl_pin, scr->cfg.bl_active_high,
                          on ? scr->brightness : 0);
}

void qg_screen_set_brightness(qg_screen_t *scr, uint8_t percent)
{
    if (scr == NULL) {
        return;
    }
    scr->brightness = (percent > 100) ? 100 : percent;
    if (scr->bl_on) {
        qg_hal_backlight_set(scr->cfg.bl_pin, scr->cfg.bl_active_high,
                              scr->brightness);
    }
}

/* ========================================================================== */
/*  Flushing (BUF8)                                                           */
/* ========================================================================== */

void qg_screen_flush(qg_screen_t *scr)
{
    if (scr == NULL || scr->backend == NULL || scr->backend->flush == NULL) return;
    scr->backend->flush(scr);
}

void qg_screen_flush_all(qg_screen_t *scr)
{
    if (scr == NULL || scr->fb == NULL) return;
    qg_int_dirty_all(scr);
    qg_screen_flush(scr);
}

/* ========================================================================== */
/*  Drawing defaults                                                          */
/* ========================================================================== */

void qg_screen_set_colors(qg_screen_t *scr, qg_color_t fg, qg_color_t bg)
{
    if (scr == NULL) {
        return;
    }
    if (fg <= 254) scr->fg_color = fg;   /* QG_DEFAULT/TRANSPARENT: unchanged */
    if (bg <= 254) scr->bg_color = bg;
}

void qg_screen_set_line_width(qg_screen_t *scr, uint8_t width)
{
    if (scr == NULL) {
        return;
    }
    scr->line_width = (width < 1) ? 1 : width;
}

/* ========================================================================== */
/*  Init                                                                      */
/* ========================================================================== */

qg_err_t qg_screen_init(qg_screen_t *scr, qg_bus_t *bus,
                          const qg_screen_config_t *cfg)
{
    if (scr == NULL || bus == NULL || cfg == NULL || !bus->ready) {
        return QG_ERR_ARG;
    }

    memset(scr, 0, sizeof(*scr));
    scr->cfg = *cfg;

    /* --- 1. Find the driver and backend ----------------------------------- */
    /* The driver comes from the config as a reference (QG_DRIVER_ST7789
     * etc.), so only the drivers a program names are linked into it.     */
    scr->drv = cfg->driver;
    if (scr->drv == NULL) {
        return QG_ERR_ARG;
    }
    /* Text history: the caller's memory (or none). */
    scr->hist     = cfg->text_history;
    scr->hist_cap = cfg->text_history ? cfg->text_history_lines : 0;

    /* The backend comes from the config as a reference, and this code never
     * names the framebuffer backend itself: that way it's only linked into
     * programs that ask for it.                                            */
    scr->backend = (cfg->backend != NULL) ? cfg->backend : &qg_backend_direct;
    if (scr->backend->needs_framebuffer) {
        /* The framebuffer holds one byte per pixel, in any rotation. */
        if (cfg->framebuffer == NULL ||
            cfg->framebuffer_size < (uint32_t)cfg->width * cfg->height) {
            return QG_ERR_ARG;
        }
        scr->fb = cfg->framebuffer;
    } else {
        scr->fb = NULL;
    }

    /* --- 2. Sanity-check the panel against the chip ----------------------- */
    /* The glass plus its offset must fit inside the chip's RAM, or the offset
     * maths in set_rotation() would go negative (and wrap to huge numbers). */
    if (cfg->width == 0 || cfg->height == 0 ||
        cfg->width  + cfg->x_offset > scr->drv->ram_w ||
        cfg->height + cfg->y_offset > scr->drv->ram_h) {
        return QG_ERR_ARG;
    }

    /* --- 3. Join the bus -------------------------------------------------- */
    qg_err_t err = qg_hal_device_init(&scr->dev, bus, cfg->cs_pin, cfg->spi_hz);
    if (err != QG_OK) {
        return err;
    }

    /* --- 4. Backlight pin: set it up, but keep it OFF until the screen is
     *        clean, so nobody sees power-up noise.                         */
    scr->brightness = 100;
    scr->bl_on      = false;
    qg_hal_backlight_init(cfg->bl_pin, cfg->bl_active_high);

    /* --- 5. Reset (software, only if there is no hardware reset line) ----- */
    /* With a shared RST line, qg_bus_init() already reset every chip, and a
     * second reset would only cost time. Without one, the chip's registers
     * are in an unknown state, so reset it by command. It gets its own CS
     * frame, and CS is high during the 150 ms the chip needs to recover.   */
    if (bus->rst_pin < 0) {
        qg_hal_begin(&scr->dev);
        qg_hal_write_cmd(&scr->dev, QG_CMD_SWRESET);
        qg_hal_end(&scr->dev);
        sleep_ms(150);
    }

    /* --- 6. Chip power-up sequence ---------------------------------------- */
    qg_driver_run_init(&scr->dev, scr->drv);

    /* --- 7. Panel settings: inversion, then rotation/BGR/mirroring -------- */
    qg_hal_begin(&scr->dev);
    qg_hal_write_cmd(&scr->dev, cfg->invert ? QG_CMD_INVON : QG_CMD_INVOFF);
    qg_hal_end(&scr->dev);

    err = qg_screen_set_rotation(scr, cfg->rotation);
    if (err != QG_OK) {
        return err;
    }

    /* --- 8. Library state ------------------------------------------------- */
    qg_palette_copy_standard(scr->palette);
    scr->fg_color   = QG_WHITE;
    scr->bg_color   = QG_BLACK;
    scr->line_width = 1;
    scr->line_style = 0xFFFF;
    scr->text_bg    = QG_TRANSPARENT;
    scr->tab_width  = QG_TAB_WIDTH;
    scr->wrap       = true;
    scr->scroll     = true;
    scr->ready    = true;     /* must be set before qg_cls() will run */

    /* --- 9. Clear the RAM, THEN show it ----------------------------------- */
    qg_cls(scr, QG_BLACK);
    qg_screen_flush(scr);      /* BUF8: the clear happened in RAM; send it */

    qg_hal_begin(&scr->dev);
    qg_hal_write_cmd(&scr->dev, QG_CMD_DISPON);
    qg_hal_end(&scr->dev);
    sleep_ms(20);

    qg_screen_backlight(scr, true);
    return QG_OK;
}
