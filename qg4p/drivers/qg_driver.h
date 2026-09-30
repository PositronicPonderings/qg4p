/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    qg_driver.h
 * @brief   Panel-driver interface: what differs between controller chips.
 *
 * LAYER:   Drivers (sits on the HAL; used by qg_screen.c and the backends)
 * DEPENDS: hal/qg_hal.h
 *
 * ---------------------------------------------------------------------------
 *  CHIP vs PANEL - TWO DIFFERENT THINGS
 * ---------------------------------------------------------------------------
 *  A display module is a *controller chip* (ST7789, ILI9341, ST7796) bonded
 *  to a *glass panel*. They are specified separately, and it helps to keep
 *  them separate in code too:
 *
 *   CHIP decides (lives here, in the driver):
 *     - the command set and its power-up sequence
 *     - the size of its internal video memory ("RAM"), e.g. 240x320
 *     - its maximum SPI speed
 *
 *   PANEL decides (lives in qg_screen_config_t, set per board):
 *     - the visible resolution (may be smaller than the chip's RAM)
 *     - where the visible area sits inside that RAM (x/y offset)
 *     - whether red and blue are swapped (RGB vs BGR wiring)
 *     - whether colours come out inverted (a photo negative)
 *     - whether the image is mirrored
 *
 *  So the same ST7789 driver serves both the 2.0" 240x320 board and the
 *  1.54" 240x240 board. Only the panel settings change.
 *
 * ---------------------------------------------------------------------------
 *  MIPI DCS: THE SHARED COMMAND SET
 * ---------------------------------------------------------------------------
 *  All three chips follow an industry standard called MIPI DCS for their core
 *  commands. That is why one set of window/rotation code works for all of
 *  them, and why only the power-up sequence needs to be chip-specific.
 */
#ifndef QG_DRIVER_H
#define QG_DRIVER_H

#include "qg_types.h"
#include "hal/qg_hal.h"

/* -------------------------------------------------------------------------- */
/*  Standard MIPI DCS commands (identical on ST7789, ILI9341 and ST7796)       */
/* -------------------------------------------------------------------------- */
#define QG_CMD_NOP      0x00  /**< Do nothing.                                */
#define QG_CMD_SWRESET  0x01  /**< Software reset (only the selected chip).   */
#define QG_CMD_SLPOUT   0x11  /**< Leave sleep mode (wake the chip up).       */
#define QG_CMD_NORON    0x13  /**< Normal display mode (not partial).        */
#define QG_CMD_INVOFF   0x20  /**< Colour inversion off.                      */
#define QG_CMD_INVON    0x21  /**< Colour inversion on.                       */
#define QG_CMD_DISPOFF  0x28  /**< Panel output off (RAM kept).               */
#define QG_CMD_DISPON   0x29  /**< Panel output on.                           */
#define QG_CMD_CASET    0x2A  /**< Column address set: x start, x end.        */
#define QG_CMD_RASET    0x2B  /**< Row address set:    y start, y end.        */
#define QG_CMD_RAMWR    0x2C  /**< Memory write: pixel data follows.          */
#define QG_CMD_MADCTL   0x36  /**< Memory access control: rotation/mirror/BGR.*/
#define QG_CMD_COLMOD   0x3A  /**< Pixel format (0x55 = 16-bit RGB565).       */

/* -------------------------------------------------------------------------- */
/*  MADCTL bits - how the chip maps its RAM onto the glass                    */
/* -------------------------------------------------------------------------- */
#define QG_MADCTL_MY   0x80  /**< Mirror rows (flip top <-> bottom).          */
#define QG_MADCTL_MX   0x40  /**< Mirror columns (flip left <-> right).       */
#define QG_MADCTL_MV   0x20  /**< Swap rows and columns (turns 90 degrees).   */
#define QG_MADCTL_ML   0x10  /**< Vertical refresh order (rarely needed).     */
#define QG_MADCTL_BGR  0x08  /**< Blue-green-red subpixel order.              */

/* -------------------------------------------------------------------------- */
/*  Init tables                                                               */
/* -------------------------------------------------------------------------- */

/**
 * One step of a chip's power-up sequence: send `cmd`, then `len` argument
 * bytes, then wait `delay_ms`.
 *
 * WHY A TABLE INSTEAD OF CODE?  Init sequences come from datasheets as lists
 * of commands and values. Keeping them as data lets you compare the table
 * line by line against the datasheet, and fix a chip by editing numbers
 * rather than logic.
 */
typedef struct {
    uint8_t  cmd;
    uint8_t  len;       /**< Number of bytes used in data[] (0..16). */
    uint16_t delay_ms;  /**< Pause after this step.                  */
    uint8_t  data[16];
} qg_init_cmd_t;

/** Everything the library needs to know about one controller chip. */
typedef struct qg_driver {
    const char           *name;         /**< For debug messages.                  */
    const qg_init_cmd_t *init_seq;     /**< Power-up steps, run in order.        */
    uint8_t               init_count;   /**< Number of steps in init_seq.         */
    uint16_t              ram_w;        /**< Chip RAM width  (native portrait).   */
    uint16_t              ram_h;        /**< Chip RAM height (native portrait).   */
    uint32_t              max_write_hz; /**< Datasheet write speed, or 0 if not
                                             verified. Informational only: many
                                             modules run faster in practice.   */
} qg_driver_t;

/* The drivers that exist so far. (Programs name them through QG_DRIVER_...
 * in qg_types.h, which is what decides which ones get linked.)             */
extern const qg_driver_t qg_drv_st7789;
extern const qg_driver_t qg_drv_ili9341;
extern const qg_driver_t qg_drv_st7796;

/* -------------------------------------------------------------------------- */
/*  Generic driver functions (work for every MIPI DCS chip)                   */
/* -------------------------------------------------------------------------- */


/** Run a chip's init table. Opens and closes its own transaction. */
void qg_driver_run_init(qg_hal_device_t *dev, const qg_driver_t *drv);

/**
 * Build the MADCTL byte for a rotation plus the panel's quirks.
 *
 * @param rot       requested rotation
 * @param mirror_x  panel is wired mirrored left/right (flip MX)
 * @param mirror_y  panel is wired mirrored top/bottom (flip MY)
 * @param bgr       panel uses blue-green-red subpixel order
 */
uint8_t qg_driver_madctl(qg_rotation_t rot, bool mirror_x, bool mirror_y, bool bgr);

/**
 * Tell the chip which rectangle of its RAM the next pixels fill, then issue
 * "memory write". Coordinates are *chip RAM* coordinates (panel offsets
 * already added) and are inclusive: x0..x1, y0..y1.
 *
 * The chip then fills that rectangle left-to-right, top-to-bottom, wrapping
 * by itself. That is why a whole filled rectangle can be sent as one long
 * stream of pixels with no further addressing.
 *
 * Must be called inside a qg_hal_begin()/qg_hal_end() transaction.
 */
void qg_driver_set_window(qg_hal_device_t *dev,
                           uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);

#endif /* QG_DRIVER_H */
