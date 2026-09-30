/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    qg_types.h
 * @brief   Shared types, enums, colour constants and error codes.
 *
 * LAYER:   Types (used by every layer)
 * DEPENDS: <stdint.h>, <stdbool.h>
 *
 * NAMING CONVENTIONS (LVGL-like, used throughout the library)
 *   qg_xxx_t          types          e.g. qg_color_t, qg_screen_t
 *   qg_module_verb()  functions      e.g. qg_screen_init(), qg_cls()
 *   QG_UPPER_CASE     constants      e.g. QG_RED, QG_ROT_90
 */
#ifndef QG_TYPES_H
#define QG_TYPES_H

#include <stdint.h>
#include <stdbool.h>

/* ========================================================================== */
/*  Colours                                                                   */
/* ========================================================================== */

/**
 * A colour, as used by every drawing call.
 *
 *   0..254          an index into the screen's 256-entry palette
 *   255             QG_TRANSPARENT  - "draw nothing"
 *   0x100           QG_DEFAULT      - "use the current/default colour"
 *
 * WHY 16 BITS FOR AN 8-BIT INDEX?  So that QG_DEFAULT can exist as a value
 * that is *not* a palette index. Two bytes per argument costs nothing on an
 * ARM CPU (arguments travel in 32-bit registers anyway).
 */
typedef uint16_t qg_color_t;

#define QG_TRANSPARENT ((qg_color_t)255)   /**< Draw nothing (unfilled, see-through). */
#define QG_DEFAULT     ((qg_color_t)0x100) /**< Use the current/default colour.       */
#define QG_NONE        ((qg_color_t)0xFFFF)/**< "No colour": qg_point() when it can't
                                                  read a pixel (DIRECT, or off-screen). */

/**
 * The 16 classic QuickBasic / CGA / VGA colours. These are palette indices
 * 0..15 of the standard palette (see qg_palette.c for their RGB values).
 */
enum {
    QG_BLACK = 0,
    QG_BLUE,
    QG_GREEN,
    QG_CYAN,
    QG_RED,
    QG_MAGENTA,
    QG_BROWN,
    QG_LIGHTGRAY,
    QG_DARKGRAY,
    QG_LIGHTBLUE,
    QG_LIGHTGREEN,
    QG_LIGHTCYAN,
    QG_LIGHTRED,
    QG_LIGHTMAGENTA,
    QG_YELLOW,
    QG_WHITE           /* = 15 */
};

/**
 * Pack 8-bit R, G, B into the 16-bit "RGB565" format the displays use.
 *
 *   bit: 15 14 13 12 11 | 10  9  8  7  6  5 | 4  3  2  1  0
 *        R  R  R  R  R  | G  G  G  G  G  G  | B  B  B  B  B
 *
 * We keep the top 5 bits of red, top 6 of green (the eye is most sensitive
 * to green, so it gets the extra bit) and top 5 of blue.
 */
#define QG_RGB565(r, g, b) \
    ((uint16_t)((((uint16_t)(r) & 0xF8u) << 8) | \
                (((uint16_t)(g) & 0xFCu) << 3) | \
                (((uint16_t)(b)) >> 3)))

/* ========================================================================== */
/*  Hardware selection enums                                                  */
/* ========================================================================== */

/*
 * Which controller chip drives a screen, chosen per screen at init (the
 * config's .driver):
 *   QG_DRIVER_ST7789    e.g. 2.0" 240x320 and 1.54" 240x240 boards
 *   QG_DRIVER_ILI9341   e.g. 2.8" 240x320 boards
 *   QG_DRIVER_ST7796    e.g. 3.5" 320x480 boards (ST7796S)
 *
 * Like the backends, these are references to the drivers themselves, so a
 * program only includes the drivers it names: pay for what you use. (A new
 * chip's driver declares itself the same way; see drivers/qg_driver.h.)
 */
struct qg_driver;
extern const struct qg_driver qg_drv_st7789, qg_drv_ili9341, qg_drv_st7796;
#define QG_DRIVER_ST7789   (&qg_drv_st7789)
#define QG_DRIVER_ILI9341  (&qg_drv_ili9341)
#define QG_DRIVER_ST7796   (&qg_drv_st7796)

/*
 * Where drawing goes, chosen per screen at init (the config's .backend):
 *   QG_BACKEND_DIRECT  straight to the panel. The everyday mode (and what a
 *                      config that doesn't say gets).
 *   QG_BACKEND_BUF8    an 8-bit framebuffer in RAM, shown by qg_screen_flush().
 *
 * These are references to the backends themselves, not numbers, and that's
 * deliberate: a program only includes the framebuffer code (and its 4 KB of
 * flush buffers) if it actually mentions QG_BACKEND_BUF8. Pay for what you use.
 */
struct qg_backend;
extern const struct qg_backend qg_backend_direct, qg_backend_buf8;
#define QG_BACKEND_DIRECT  (&qg_backend_direct)
#define QG_BACKEND_BUF8    (&qg_backend_buf8)

/**
 * Screen rotation. The *exact* direction (clockwise vs counter-clockwise)
 * depends on how the glass is bonded to the controller; the demos'
 * orientation test shows you which way yours turns.
 */
typedef enum {
    QG_ROT_0 = 0,
    QG_ROT_90,
    QG_ROT_180,
    QG_ROT_270
} qg_rotation_t;

/** Use for any optional pin that is not connected (e.g. no reset line). */
#define QG_PIN_NONE (-1)

/* ========================================================================== */
/*  Error codes                                                               */
/* ========================================================================== */

/** Every function that can fail returns one of these. 0 always means OK. */
typedef enum {
    QG_OK              =  0,
    QG_ERR_ARG         = -1, /**< A bad argument (NULL pointer, bad size...).  */
    QG_ERR_UNSUPPORTED = -2, /**< Feature/driver not available (yet).          */
    QG_ERR_OVERFLOW    = -4  /**< Ran out of working space (e.g. qg_paint).   */
} qg_err_t;

/**
 * Colour adjustment for one screen: see qg_screen_set_color_adjust().
 * Index [0] is red, [1] green, [2] blue.
 */
typedef struct {
    uint8_t  gain[3];     /**< Brightness of full intensity, percent (100 = unchanged). */
    uint16_t gamma[3];    /**< Mid-tone curve x 100 (100 = unchanged; higher = darker mid-tones). */
} qg_color_adjust_t;

/** No adjustment: all gains 100 %, all gammas 1.00. */
#define QG_COLOR_ADJUST_NONE { { 100, 100, 100 }, { 100, 100, 100 } }

/**
 * The working tables for one adjusted screen (1,280 bytes). You provide
 * one per screen you adjust, like a framebuffer:
 *     static qg_color_adjust_state_t adj_state;
 * Screens you don't adjust need none, and cost nothing.
 */
typedef struct {
    qg_color_adjust_t settings;        /**< As given.                          */
    uint8_t           lut[3][256];     /**< 8-bit in -> 8-bit out, per channel. */
    uint16_t          out[256];        /**< The screen's palette AS SENT.       */
} qg_color_adjust_state_t;

#endif /* QG_TYPES_H */
