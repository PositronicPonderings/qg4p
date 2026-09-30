/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    qg_screen.h
 * @brief   Screen objects: one per physical display.
 *
 * LAYER:   Public API (screens)
 * DEPENDS: qg_types.h, hal/qg_hal.h, drivers/qg_driver.h, backend/qg_backend.h
 *
 * Every drawing call takes a screen as its first argument:
 *
 *     qg_cls(&scr, QG_BLUE);
 *
 * A screen owns everything specific to one display: which chip drives it,
 * its CS pin and SPI speed, its panel quirks, its rotation, its palette and
 * its text cursor and fonts, and its optional framebuffer.
 */
#ifndef QG_SCREEN_H
#define QG_SCREEN_H

#include "qg_types.h"
#include "hal/qg_hal.h"
#include "drivers/qg_driver.h"
#include "backend/qg_backend.h"

struct qg_font;   /* defined in qg_text.h */

/** One remembered line of text, for scrolling DIRECT screens (see qg_text.c). */
typedef struct {
    int16_t  y;                              /**< Top of the line on screen.   */
    int16_t  h;                              /**< Line height.                 */
    uint16_t len;                            /**< Characters used in text[].   */
    char     text[QG_TEXT_HISTORY_CHARS];   /**< Markup that redraws the line.*/
} qg_text_line_t;

/**
 * Everything needed to bring up one screen. Fill one in per display.
 *
 * PANEL SETTINGS: HOW TO FIND THE RIGHT VALUES
 *   Run demo/m1_demo.c (or m0_demo.c) and watch the screen:
 *     - First colour looks WHITE instead of BLACK  -> flip `invert`
 *     - RED shows as BLUE (and BLUE as RED)        -> flip `bgr`
 *     - Picture is mirrored                        -> flip mirror_x/mirror_y
 *     - Border line missing / garbage strip at one edge -> check the offsets
 */
typedef struct {
    /* --- chip & wiring --- */
    const struct qg_driver *driver; /**< Controller chip: QG_DRIVER_ST7789 etc.  */
    int8_t             cs_pin;      /**< This screen's chip-select GPIO.         */
    int8_t             bl_pin;      /**< Backlight GPIO, or QG_PIN_NONE if the
                                         backlight is wired straight to power.   */
    bool               bl_active_high; /**< true: HIGH = backlight on.          */
    uint32_t           spi_hz;      /**< Requested SPI speed for this screen.    */

    /* --- panel (the glass) --- */
    uint16_t           width;       /**< Visible width  in native (portrait) orientation. */
    uint16_t           height;      /**< Visible height in native (portrait) orientation. */
    uint16_t           x_offset;    /**< Where the glass starts in chip RAM (rotation 0). */
    uint16_t           y_offset;
    bool               bgr;         /**< Subpixels wired blue-green-red.         */
    bool               invert;      /**< Panel needs colour inversion ON.        */
    bool               mirror_x;    /**< Glass mounted mirrored left/right.      */
    bool               mirror_y;    /**< Glass mounted mirrored top/bottom.      */

    /* --- library behaviour --- */
    qg_rotation_t     rotation;    /**< Initial rotation.                       */
    const struct qg_backend *backend; /**< QG_BACKEND_DIRECT (or NULL), or QG_BACKEND_BUF8. */

    /* --- BUF8 only: the framebuffer, which YOU provide ---
     * One byte per pixel: width x height bytes, e.g.
     *     static uint8_t my_fb[320 * 480];          (150 KB)
     * Ignored for DIRECT screens.                                           */
    uint8_t           *framebuffer;
    uint32_t           framebuffer_size;   /**< in bytes                     */

    /* --- DIRECT screens that scroll text: the lines to reprint, YOURS ---
     * e.g. static qg_text_line_t history[QG_TEXT_HISTORY_LINES];
     * NULL: printing past the bottom clears and starts again at the top.
     * (See QG_TEXT_HISTORY_LINES in qg_config.h.)                          */
    qg_text_line_t    *text_history;
    uint8_t            text_history_lines;
} qg_screen_config_t;

/**
 * Runtime state of one screen. Allocate it yourself (usually a static/global)
 * and pass it to qg_screen_init(). Read-only for application code, except
 * where a qg_ function says otherwise.
 */
struct qg_screen {
    qg_screen_config_t   cfg;       /**< Copy of the config used at init.       */
    const qg_driver_t   *drv;       /**< Chip driver.                           */
    qg_hal_device_t      dev;       /**< This screen's slot on the SPI bus.     */
    const qg_backend_t  *backend;   /**< Where pixels go.                       */

    qg_rotation_t        rotation;  /**< Current rotation.                      */
    int16_t               width;     /**< Current width  (after rotation).       */
    int16_t               height;    /**< Current height (after rotation).       */
    uint16_t              ram_x_off; /**< Current offsets into chip RAM, worked  */
    uint16_t              ram_y_off; /**< out for the current rotation.          */

    uint16_t              palette[256]; /**< This screen's palette, RGB565.      */
    uint16_t              palette_gen;  /**< Bumped on every palette change.     */

    /* --- colour adjustment: NULL unless qg_screen_set_color_adjust() --- */
    qg_color_adjust_state_t *adjust;   /**< Your storage, or NULL (no cost).   */

    /* --- framebuffer (BUF8 screens only) --- */
    uint8_t              *fb;        /**< Framebuffer, width x height bytes.     */
    bool                  dirty;     /**< Anything changed since the last flush? */
    int16_t               dirty_x0, dirty_y0, dirty_x1, dirty_y1; /**< Changed area (inclusive). */
    qg_color_t           fg_color;  /**< QG_DEFAULT for lines/strokes/fills.   */
    qg_color_t           bg_color;  /**< QG_DEFAULT for qg_cls().             */
    uint8_t               line_width;/**< Stroke thickness in pixels (>= 1).     */
    uint16_t              line_style;/**< LINE style bit pattern (0xFFFF = solid).*/

    /* --- the view (QuickBasic's VIEW); the whole screen when not set --- */
    int16_t               view_x0, view_y0;  /**< Top-left, screen pixels.       */
    int16_t               view_x1, view_y1;  /**< One past bottom-right.         */
    int16_t               origin_x, origin_y;/**< Added to every coordinate.     */
    bool                  view_moved;        /**< Origin moved to the view?      */

    uint8_t               brightness; /**< Backlight level 0..100 (%).           */
    bool                  bl_on;     /**< Backlight switched on?                 */

    /* --- text --- */
    int16_t               cursor_x;  /**< Print cursor: top-left of the next char.*/
    int16_t               cursor_y;
    int16_t               margin_x;  /**< Where a new line starts (set by locate).*/
    qg_color_t           text_bg;   /**< QG_TRANSPARENT, or an opaque colour.  */
    const struct qg_font *fonts[QG_MAX_FONTS]; /**< Slot 0 = default font.   */
    int16_t               tab_width; /**< Pixels between tab stops.              */
    int16_t               last_line_h; /**< Height of the last printed line.     */
    bool                  wrap;      /**< Word-wrap at the right edge.           */
    bool                  scroll;    /**< Scroll up at the bottom.               */
    qg_text_line_t       *hist;      /**< Your line memory, or NULL.             */
    uint8_t               hist_cap;  /**< How many lines it holds.               */
    uint8_t               hist_count;
    bool                  hist_open; /**< Last line may still be added to.       */

    bool                  ready;     /**< Set once init has succeeded.           */
};

/**
 * Bring a screen to life: run the chip's init table, apply the panel
 * settings and rotation, load the standard palette, clear to black, switch
 * the display and backlight on.
 *
 * The bus must already be set up with qg_bus_init().
 *
 * @return QG_OK, or QG_ERR_ARG (a bad config, including no driver, or a
 *         framebuffer backend without a framebuffer).
 */
qg_err_t qg_screen_init(qg_screen_t *scr, qg_bus_t *bus,
                          const qg_screen_config_t *cfg);

/**
 * Change rotation. The screen's width/height swap for 90/270. Existing pixels
 * are NOT moved; clear or redraw afterwards. (On a BUF8 screen, the
 * framebuffer's contents are re-read in the new shape, so they look
 * scrambled until you redraw.)
 */
qg_err_t qg_screen_set_rotation(qg_screen_t *scr, qg_rotation_t rot);

/**
 * Switch the backlight on or off. Switching on restores the last brightness
 * set with qg_screen_set_brightness() (100% after init), so "off then on"
 * doesn't lose a dimmed setting. No effect if bl_pin is QG_PIN_NONE.
 */
void qg_screen_backlight(qg_screen_t *scr, bool on);

/**
 * Set backlight brightness, 0..100 percent. Takes effect immediately if the
 * backlight is on, otherwise the next time it's switched on.
 */
void qg_screen_set_brightness(qg_screen_t *scr, uint8_t percent);

/** Current brightness setting, 0..100 percent. */
static inline uint8_t qg_screen_get_brightness(const qg_screen_t *scr) { return scr->brightness; }

/**
 * Set the screen's default colours. QuickBasic: COLOR fg, bg
 *   fg - used wherever a drawing call is given QG_DEFAULT (lines, outlines,
 *        fills, and text)
 *   bg - used by qg_cls(scr, QG_DEFAULT)
 * Pass QG_DEFAULT for either one to leave it unchanged.
 * After init: fg = QG_WHITE, bg = QG_BLACK.
 */
void qg_screen_set_colors(qg_screen_t *scr, qg_color_t fg, qg_color_t bg);

/**
 * Set the stroke thickness, in pixels, for lines, box/circle/ellipse
 * outlines and arcs. 1 after init. Values below 1 act as 1.
 *
 * WHERE THE THICKNESS GOES
 *   Lines:                 centred on the line.
 *   Box/circle/ellipse:    inward, inside the shape's edge, so a shape's
 *                          outer size never depends on its line width.
 *                          (This matches how CSS draws borders.)
 *   Arcs:                  inward, like circle outlines.
 */
void qg_screen_set_line_width(qg_screen_t *scr, uint8_t width);

static inline uint8_t qg_screen_get_line_width(const qg_screen_t *scr) { return scr->line_width; }

/**
 * BUF8 screens: send everything drawn since the last flush to the panel.
 * Only the rectangle that changed is sent. Does nothing on DIRECT screens
 * (they're always up to date), so it's safe to call on any screen.
 */
void qg_screen_flush(qg_screen_t *scr);

/** BUF8 screens: send the whole screen, changed or not. */
void qg_screen_flush_all(qg_screen_t *scr);

/** True if this screen draws into a framebuffer (BUF8). */
static inline bool qg_screen_is_buffered(const qg_screen_t *scr) { return scr->fb != NULL; }

/** Current width/height in pixels (these change with rotation). */
static inline int16_t qg_screen_width(const qg_screen_t *scr)  { return scr->width;  }
static inline int16_t qg_screen_height(const qg_screen_t *scr) { return scr->height; }

#endif /* QG_SCREEN_H */
