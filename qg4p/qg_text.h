/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    qg_text.h
 * @brief   Fonts and printing text.
 *
 * LAYER:   Public API (text)
 * DEPENDS: qg_screen.h, qg_font_format.h
 *
 * ---------------------------------------------------------------------------
 *  FONTS IN THREE STEPS
 * ---------------------------------------------------------------------------
 *   1. Font DATA is a const table in flash, from tools/ttf2qg.py or LVGL's
 *      converter. Three come with the library (qg4p/fonts/):
 *          qg_font_mono_12        DejaVu Sans Mono, 12 px
 *          qg_font_sans_16        DejaVu Sans, 16 px
 *          qg_font_sans_bold_24   DejaVu Sans Bold, 24 px
 *      Each covers printable ASCII plus the symbols ° ± ×.
 *
 *   2. A FONT (qg_font_t) pairs that data with a default colour and scale:
 *          static qg_font_t body  = QG_FONT_INIT(qg_font_sans_16, QG_DEFAULT, 1);
 *          static qg_font_t title = QG_FONT_INIT(qg_font_sans_bold_24, QG_YELLOW, 2);
 *      Make these static or global: screens keep a pointer to them.
 *      (qg_font_create() does the same at run time.)
 *
 *   3. Give fonts to a screen by SLOT. Slot 0 is the default for printing;
 *      other slots are picked with {f:n} markup:
 *          qg_screen_set_font(&scr, 0, &body);
 *          qg_screen_set_font(&scr, 1, &title);
 *
 * ---------------------------------------------------------------------------
 *  PRINTING (QuickBasic: LOCATE, PRINT)
 * ---------------------------------------------------------------------------
 *   qg_locate(scr, x, y)     move the print cursor. It also sets the left
 *                             margin: new lines start at this x.
 *   qg_print(scr, "text")    print at the cursor, cursor moves on
 *   qg_println(scr, "text")  the same, then start a new line
 *   qg_print_at(scr, x, y, "text", color, font)
 *                             one-off text anywhere; the cursor stays put
 *
 *   qg_print_align(scr, y, "text", QG_ALIGN_CENTER)
 *                             each line centred (or left/right) on the screen
 *   qg_print_box(scr, x, y, width, "text", align)
 *                             wrapped and aligned inside a column
 *
 *   Coordinates are the TOP-LEFT of the text's line. Text is UTF-8, so "°"
 *   in your source works as long as the font includes it; a missing
 *   character prints as "?".
 *
 * ---------------------------------------------------------------------------
 *  ESCAPES AND MARKUP
 * ---------------------------------------------------------------------------
 *   \n        new line, back to the margin
 *   \r        back to the margin on the same line
 *   \t        jump to the next tab stop (every tab_width pixels from the
 *             margin; see qg_screen_set_tab_width)
 *   {c:RED}   colour by name (the 16 named colours, any case) ...
 *   {c:200}   ... or by palette index
 *   {f:1}     switch to font slot 1 (its size, and its colour unless a
 *             {c:} is active)
 *   {s:2}     scale 2 (1..4)
 *   {x:120}   move to pixel column 120 (handy for lining up columns)
 *   {c:} {f:} {s:}   back to the default for this print
 *   {{        a literal "{"
 *
 *   Markup lasts until the end of the print call; every call starts fresh.
 *   Unknown tags are skipped. A "{" with no closing "}" prints as-is.
 *
 *     qg_println(&scr, "Rolled {c:LIGHTGREEN}{s:2}20{s:}{c:} - {f:1}critical!");
 *
 * ---------------------------------------------------------------------------
 *  WRAPPING AND SCROLLING
 * ---------------------------------------------------------------------------
 *   Text wraps at word boundaries when it reaches the screen's right edge
 *   (a word too long for a whole line is split). Turn off with
 *   qg_screen_set_wrap(scr, false).
 *
 *   When qg_print/qg_println reach the bottom of the screen, the screen
 *   scrolls up, QuickBasic style. On a DIRECT screen the library remembers
 *   the lines printed with qg_print/qg_println and reprints them one row
 *   higher; anything else on that screen (shapes, qg_print_at text) is
 *   cleared by a scroll. qg_print_at never scrolls. Turn off with
 *   qg_screen_set_scroll(scr, false).
 *
 *   Mixing font sizes on one line lines them all up on a shared baseline,
 *   and the line is as tall as its tallest font.
 *
 * ---------------------------------------------------------------------------
 *  COLOURS
 * ---------------------------------------------------------------------------
 *   Text colour, first match wins:
 *     1. the colour passed to qg_print_at() (if not QG_DEFAULT)
 *     2. the font's own colour (if not QG_DEFAULT)
 *     3. the screen's foreground colour (qg_screen_set_colors)
 *
 *   Background: transparent by default, so text over a shape just works.
 *   qg_screen_set_text_bg(scr, colour) makes it opaque: each character's
 *   whole cell is painted, which is faster on DIRECT screens and lets you
 *   overwrite a changing number without clearing it first.
 */
#ifndef QG_TEXT_H
#define QG_TEXT_H

#include "qg_screen.h"
#include "qg_font_format.h"

/* The fonts that come with the library. */
LV_FONT_DECLARE(qg_font_mono_12)
LV_FONT_DECLARE(qg_font_sans_16)
LV_FONT_DECLARE(qg_font_sans_bold_24)

/** Horizontal alignment for qg_print_align(). */
typedef enum {
    QG_ALIGN_LEFT = 0,
    QG_ALIGN_CENTER,
    QG_ALIGN_RIGHT
} qg_align_t;

/** A font: its data plus a default colour and scale. */
typedef struct qg_font {
    const lv_font_t *data;    /**< The glyph tables.                          */
    qg_color_t      color;   /**< Default text colour, or QG_DEFAULT.       */
    uint8_t          scale;   /**< 1..4: each font pixel becomes scale x scale.*/
} qg_font_t;

/**
 * Build a font object at compile time, for static/global variables:
 *     static qg_font_t body = QG_FONT_INIT(qg_font_sans_16, QG_DEFAULT, 1);
 */
#define QG_FONT_INIT(font_data, color_, scale_) \
    { .data = &(font_data), .color = (color_), .scale = (scale_) }

/**
 * Make a font object at run time. Scale is clamped to 1..4.
 * Example: qg_font_t f = qg_font_create(&qg_font_sans_16, QG_WHITE, 1);
 */
qg_font_t qg_font_create(const lv_font_t *data, qg_color_t color, uint8_t scale);

/**
 * Check that font data is in a form this library can draw
 * (1 bit per pixel, uncompressed).
 * @return QG_OK, QG_ERR_ARG (NULL/empty) or QG_ERR_UNSUPPORTED.
 */
qg_err_t qg_font_check(const lv_font_t *data);

/** Height of one line of this font, in pixels, including its scale. */
int16_t qg_font_line_height(const qg_font_t *font);

/**
 * Put a font in one of a screen's slots (0 .. QG_MAX_FONTS-1). Pass NULL to
 * empty a slot. The font object must stay valid (make it static or global).
 * @return QG_OK, QG_ERR_ARG or QG_ERR_UNSUPPORTED (see qg_font_check).
 */
qg_err_t qg_screen_set_font(qg_screen_t *scr, uint8_t slot, const qg_font_t *font);

/** Text background: QG_TRANSPARENT (default) or an opaque palette colour. */
void qg_screen_set_text_bg(qg_screen_t *scr, qg_color_t bg);

/** Pixels between tab stops, measured from the margin (default 40). */
void qg_screen_set_tab_width(qg_screen_t *scr, int16_t pixels);

/** Word-wrap at the screen's right edge (default on). */
void qg_screen_set_wrap(qg_screen_t *scr, bool on);

/** Scroll up when qg_print/qg_println pass the bottom (default on). */
void qg_screen_set_scroll(qg_screen_t *scr, bool on);

/** Print cursor position, in pixels (relative to the view's origin).
 *  QuickBasic: POS(0) and CSRLIN, which counted characters, not pixels. */
static inline int16_t qg_pos(const qg_screen_t *scr)    { return scr->cursor_x; }
static inline int16_t qg_csrlin(const qg_screen_t *scr) { return scr->cursor_y; }

/** Move the print cursor, and set the left margin for new lines. */
void qg_locate(qg_screen_t *scr, int16_t x, int16_t y);

/** qg_locate() in percentages of the screen (see qg_draw_pct.h). */
void qg_locate_pct(qg_screen_t *scr, uint8_t x, uint8_t y);

/** Print at the cursor with the slot 0 font; the cursor moves on. */
void qg_print(qg_screen_t *scr, const char *text);

/** qg_print(), then a new line. */
void qg_println(qg_screen_t *scr, const char *text);

/**
 * Print anywhere without moving the cursor. "\n" in the text returns to x.
 * @param color  text colour, or QG_DEFAULT (see COLOURS above)
 * @param font   a font, or NULL for the screen's slot 0 font
 */
void qg_print_at(qg_screen_t *scr, int16_t x, int16_t y, const char *text,
                  qg_color_t color, const qg_font_t *font);

/**
 * Print with each line aligned left, centred or right across the screen,
 * starting at row y. Wraps per qg_screen_set_wrap. Doesn't move the cursor.
 */
void qg_print_align(qg_screen_t *scr, int16_t y, const char *text, qg_align_t align);

/**
 * Print inside a column: text wraps to fit `width` pixels starting at x, and
 * each line is aligned within that width. Pairs with qg_text_measure():
 * measure with the same width to size a box, then print into it.
 * Uses the slot 0 font. Doesn't move the cursor.
 */
void qg_print_box(qg_screen_t *scr, int16_t x, int16_t y, int16_t width,
                   const char *text, qg_align_t align);

/**
 * Measure text without drawing it: the width of its widest line and the
 * total height of all its lines, in pixels. Markup, tabs and scaling are
 * all taken into account, exactly as printing would.
 *
 * @param font       a font, or NULL for the screen's slot 0 font
 * @param max_width  wrap at this width, as printing would; 0 = don't wrap
 * @param w, h       results (either may be NULL)
 */
void qg_text_measure(qg_screen_t *scr, const char *text, const qg_font_t *font,
                      int16_t max_width, int16_t *w, int16_t *h);

#endif /* QG_TEXT_H */
