/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    qg_drv_st7796.c
 * @brief   Sitronix ST7796S controller.
 *
 * LAYER:   Drivers
 * DEPENDS: drivers/qg_driver.h
 *
 * Boards using this driver:
 *   - 3.5" 320x480 blue board with 9-pin SPI header (IM0..IM2 all = 1)
 *
 * Chip facts:
 *   - RAM: 320 columns x 480 rows.
 *   - Write speed: runs cleanly at 37.5 MHz on this board (confirmed on
 *     breadboard wiring).
 *
 * WHERE THIS TABLE CAME FROM
 *   The widely used ST7796S sequence, as published in Bodmer's TFT_eSPI
 *   library (https://github.com/Bodmer/TFT_eSPI), minus MADCTL and DISPON,
 *   which this library sends from the panel config. (Register values like
 *   these come from the controller's datasheet and vendor sample code; that
 *   library is where they were found, collected and tested.)
 *
 * "COMMAND SET CONTROL" (0xF0)
 *   The ST7796S locks its advanced registers by default. Writing 0xC3 then
 *   0x96 to register 0xF0 unlocks them; 0x3C then 0x69 locks them again.
 *   Everything between the unlock and lock steps tunes panel voltages,
 *   timing and gamma.
 *
 * CONFIRMED PANEL SETTINGS for the 3.5" board:
 *     .bgr = true, .mirror_x = true, .invert = false
 *   Note: an early guess of invert = true showed white as black and yellow
 *   as blue, which is the signature of a wrongly inverted panel.
 */
#include "drivers/qg_driver.h"

static const qg_init_cmd_t st7796_init[] = {
    /* cmd   len delay  data                                                   */

    /* Wake up first; the chip needs 120 ms before further configuration.     */
    { QG_CMD_SLPOUT, 0, 120, { 0 } },

    /* Unlock the advanced registers.                                          */
    { 0xF0,   1,   0, { 0xC3 } },
    { 0xF0,   1,   0, { 0x96 } },

    { QG_CMD_COLMOD, 1, 0, { 0x55 } },                   /* 16-bit RGB565      */

    /* 0xB4 is "display inversion control": HOW the panel alternates its
     * drive voltage between pixels (here, 1-dot inversion). It is NOT the
     * same as INVON/INVOFF, which invert the colours.                         */
    { 0xB4,   1,   0, { 0x01 } },

    /* Display function control. Third byte 0x3B = (59+1) x 8 = 480 lines.   */
    { 0xB6,   3,   0, { 0x80, 0x02, 0x3B } },

    /* Display output ctrl adjust (source/gate timing, vendor values).        */
    { 0xE8,   8,   0, { 0x40, 0x8A, 0x00, 0x00, 0x29, 0x19, 0xA5, 0x33 } },

    { 0xC1,   1,   0, { 0x06 } },                         /* Power control 2    */
    { 0xC2,   1,   0, { 0xA7 } },                         /* Power control 3    */
    { 0xC5,   1, 120, { 0x18 } },                         /* VCOM control       */

    /* Gamma curves (positive and negative).                                   */
    { 0xE0,  14,   0, { 0xF0, 0x09, 0x0B, 0x06, 0x04, 0x15, 0x2F,
                        0x54, 0x42, 0x3C, 0x17, 0x14, 0x18, 0x1B } },
    { 0xE1,  14, 120, { 0xE0, 0x09, 0x0B, 0x06, 0x04, 0x03, 0x2B,
                        0x43, 0x42, 0x3B, 0x16, 0x14, 0x17, 0x1B } },

    /* Lock the advanced registers again.                                      */
    { 0xF0,   1,   0, { 0x3C } },
    { 0xF0,   1, 120, { 0x69 } },
};

const qg_driver_t qg_drv_st7796 = {
    .name         = "ST7796S",
    .init_seq     = st7796_init,
    .init_count   = (uint8_t)(sizeof(st7796_init) / sizeof(st7796_init[0])),
    .ram_w        = 320,
    .ram_h        = 480,
    .max_write_hz = 0u,   /* no datasheet figure recorded; 37.5 MHz tested */
};
