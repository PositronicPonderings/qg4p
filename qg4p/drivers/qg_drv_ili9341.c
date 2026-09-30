/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    qg_drv_ili9341.c
 * @brief   Ilitek ILI9341 controller.
 *
 * LAYER:   Drivers
 * DEPENDS: drivers/qg_driver.h
 *
 * Boards using this driver:
 *   - 2.8" 240x320 red board "2.8" TFT 240xRGBx320 V1.1"
 *
 * Chip facts (ILI9341 datasheet):
 *   - RAM: 240 columns x 320 rows.
 *   - Official write clock: 100 ns cycle -> 10 MHz. In practice these boards
 *     run well above that: this board runs cleanly at 37.5 MHz (confirmed
 *     hardware, breadboard wiring). Each screen sets its own speed, so a slower
 *     board never slows down the others.
 *
 * WHERE THIS TABLE CAME FROM
 *   It's the widely used vendor sequence, as published in Adafruit's
 *   Adafruit_ILI9341 library (https://github.com/adafruit/Adafruit_ILI9341)
 *   and many others. Several commands (0xEF, 0xCF, 0xED,
 *   0xE8, 0xCB, 0xF7, 0xEA) are "extended" power and timing registers. Most
 *   are documented in the datasheet's extended command section; 0xEF is not
 *   documented at all but appears in the vendor's sample code. They tune the
 *   panel's internal voltages and are the usual cure for washed-out or
 *   unstable images, so they're kept exactly as the vendor gave them.
 *
 * MOVED OUT OF THE TABLE (compared with the POC)
 *   MADCTL 0x48 -> panel config: 0x48 = MX (mirror X) + BGR, so this panel
 *                  uses  .bgr = true, .mirror_x = true.
 *   DISPON      -> sent by qg_screen_init() after the RAM is cleared.
 *   The POC sends no INVON, so this panel uses  .invert = false.
 */
#include "drivers/qg_driver.h"

static const qg_init_cmd_t ili9341_init[] = {
    /* cmd   len delay  data                                                   */

    /* --- Extended power / timing registers (vendor values) ----------------- */
    { 0xEF,   3,   0, { 0x03, 0x80, 0x02 } },             /* undocumented       */
    { 0xCF,   3,   0, { 0x00, 0xC1, 0x30 } },             /* Power control B    */
    { 0xED,   4,   0, { 0x64, 0x03, 0x12, 0x81 } },       /* Power-on sequence  */
    { 0xE8,   3,   0, { 0x85, 0x00, 0x78 } },             /* Driver timing A    */
    { 0xCB,   5,   0, { 0x39, 0x2C, 0x00, 0x34, 0x02 } }, /* Power control A    */
    { 0xF7,   1,   0, { 0x20 } },                         /* Pump ratio control */
    { 0xEA,   2,   0, { 0x00, 0x00 } },                   /* Driver timing B    */

    /* --- Panel voltages ------------------------------------------------------ */
    { 0xC0,   1,   0, { 0x23 } },                         /* Power control 1    */
    { 0xC1,   1,   0, { 0x10 } },                         /* Power control 2    */
    { 0xC5,   2,   0, { 0x3E, 0x28 } },                   /* VCOM control 1     */
    { 0xC7,   1,   0, { 0x86 } },                         /* VCOM control 2     */

    /* --- Pixel format and timing -------------------------------------------- */
    { QG_CMD_COLMOD, 1, 0, { 0x55 } },                   /* 16-bit RGB565      */
    { 0xB1,   2,   0, { 0x00, 0x18 } },                   /* Frame rate ~79 Hz  */
    { 0xB6,   3,   0, { 0x08, 0x82, 0x27 } },             /* Display function   */

    /* --- Gamma ---------------------------------------------------------------- */
    { 0xF2,   1,   0, { 0x00 } },                         /* 3-gamma off        */
    { 0x26,   1,   0, { 0x01 } },                         /* Gamma curve 1      */
    { 0xE0,  15,   0, { 0x0F, 0x31, 0x2B, 0x0C, 0x0E, 0x08, 0x4E, 0xF1,
                        0x37, 0x07, 0x10, 0x03, 0x0E, 0x09, 0x00 } }, /* + gamma */
    { 0xE1,  15,   0, { 0x00, 0x0E, 0x14, 0x03, 0x11, 0x07, 0x31, 0xC1,
                        0x48, 0x08, 0x0F, 0x0C, 0x31, 0x36, 0x0F } }, /* - gamma */

    /* --- Wake up -------------------------------------------------------------- */
    { QG_CMD_SLPOUT, 0, 120, { 0 } },
};

const qg_driver_t qg_drv_ili9341 = {
    .name         = "ILI9341",
    .init_seq     = ili9341_init,
    .init_count   = (uint8_t)(sizeof(ili9341_init) / sizeof(ili9341_init[0])),
    .ram_w        = 240,
    .ram_h        = 320,
    .max_write_hz = 10000000u,
};
