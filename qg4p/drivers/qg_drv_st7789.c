/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    qg_drv_st7789.c
 * @brief   Sitronix ST7789 / ST7789V controller.
 *
 * LAYER:   Drivers
 * DEPENDS: drivers/qg_driver.h
 *
 * Boards using this driver:
 *   - 2.0" 240x320 blue board "GMT020-02-8P VER:1.21" (8-pin header)
 *   - 1.54" 240x240 "MRD-1.54" test module
 *
 * Chip facts (ST7789V datasheet):
 *   - RAM: 240 columns x 320 rows, so a 240x240 panel only uses part of it
 *     (hence the panel offsets in qg_screen_config_t).
 *   - Write clock: 16 ns minimum period -> 62.5 MHz.
 *
 * WHAT IS *NOT* IN THIS TABLE, AND WHY
 *   MADCTL (rotation/BGR), INVON/INVOFF (inversion) and DISPON (display on)
 *   are panel decisions. qg_screen_init() sends them after this table, from
 *   the screen's config. The display is switched on only after its memory
 *   has been cleared, so you never see a flash of random garbage at power-up.
 */
#include "drivers/qg_driver.h"

static const qg_init_cmd_t st7789_init[] = {
    /* cmd               len  delay  data                                     */

    /* No software reset here. The shared RST line has already reset the
     * chip in qg_bus_init(); if a board has no RST line, qg_screen_init()
     * sends a software reset in its own transaction before this table.     */

    /* Leave sleep mode. The datasheet requires 120 ms before the next
     * "sleep in" and 5 ms before other commands; we wait the full 120.       */
    { QG_CMD_SLPOUT,     0,  120,  { 0 } },

    /* Pixel format: 0x55 = 16 bits per pixel (RGB565) on both the RGB and
     * MCU interfaces.                                                         */
    { QG_CMD_COLMOD,     1,   10,  { 0x55 } },

    /* Normal (full-screen) display mode.                                      */
    { QG_CMD_NORON,      0,   10,  { 0 } },
};

const qg_driver_t qg_drv_st7789 = {
    .name         = "ST7789",
    .init_seq     = st7789_init,
    .init_count   = (uint8_t)(sizeof(st7789_init) / sizeof(st7789_init[0])),
    .ram_w        = 240,
    .ram_h        = 320,
    .max_write_hz = 62500000u,
};
