/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    qg_font_format.h
 * @brief   The font DATA format: LVGL's "fmt_txt" layout, defined here so
 *          font files work without LVGL itself.
 *
 * LAYER:   Fonts (data format only; the drawing code is in qg_text.c)
 * DEPENDS: <stdint.h>, <stdbool.h>
 *
 * ---------------------------------------------------------------------------
 *  WHY LVGL'S FORMAT?
 * ---------------------------------------------------------------------------
 *  Turning a TrueType font into pixel glyphs is fiddly work that LVGL's font
 *  converter (and its online version) already does well. Its output is a
 *  plain .c file full of tables. By defining the same type and field names
 *  here, those .c files compile against THIS library with no LVGL present.
 *  Our own converter, tools/ttf2qg.py, writes the same layout, so fonts from
 *  either source are interchangeable.
 *
 *  A converted font file does `#include "lvgl/lvgl.h"`. The file
 *  qg4p/lvgl/lvgl.h is a two-line stand-in that includes this header.
 *  (If a project ever adds the real LVGL, remove that stand-in.)
 *
 * ---------------------------------------------------------------------------
 *  WHAT'S IN A FONT FILE (all tables, all in flash)
 * ---------------------------------------------------------------------------
 *   glyph_bitmap[]  every glyph's pixels, packed 1 bit per pixel with no
 *                   padding between rows. Each glyph starts on a byte.
 *   glyph_dsc[]     per glyph: where its bitmap starts, its box size, where
 *                   the box sits relative to the pen, how far to advance.
 *   cmaps[]         character map: Unicode code point -> glyph number.
 *   kern data       (optional) small spacing tweaks for letter pairs like
 *                   "AV" that look better nudged together.
 *
 *  SUPPORTED IN v1: 1 bit per pixel, uncompressed bitmaps (bitmap_format 0).
 *  qg_screen_set_font() rejects anything else with QG_ERR_UNSUPPORTED
 *  rather than drawing garbage. In LVGL's converter choose Bpp = 1 and turn
 *  compression off (command line: --bpp 1 --no-compress).
 *
 * ---------------------------------------------------------------------------
 *  GLYPH GEOMETRY (all in pixels, y measured UP from the baseline)
 * ---------------------------------------------------------------------------
 *
 *     pen position                           next pen position
 *        |<------------- adv_w (in 1/16 px) --------->|
 *        |     ofs_x                                  |
 *        |<--->+---------+  ^                         |
 *        |     |  glyph  |  | box_h                   |
 *        |     |   box   |  |                         |
 *   -----+-----+---------+--v-- baseline -------------+-----
 *        |           ^ ofs_y = height of the box's bottom edge above the
 *                      baseline (negative for descenders like 'g', 'y')
 *
 *   line_height  the height of one full line of text
 *   base_line    how far the baseline sits above the bottom of the line
 */
#ifndef QG_FONT_FORMAT_H
#define QG_FONT_FORMAT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* --- Version: font files test these to choose which fields to fill in. ---
 * We present ourselves as LVGL 8.3, whose font layout is the simplest.      */
#ifndef LVGL_VERSION_MAJOR
#define LVGL_VERSION_MAJOR 8
#define LVGL_VERSION_MINOR 3
#define LVGL_VERSION_PATCH 0
#endif

#ifndef LV_VERSION_CHECK
#define LV_VERSION_CHECK(x, y, z) \
    ((x) == LVGL_VERSION_MAJOR && ((y) < LVGL_VERSION_MINOR || \
     ((y) == LVGL_VERSION_MINOR && (z) <= LVGL_VERSION_PATCH)))
#endif

#define LV_ATTRIBUTE_LARGE_CONST          /* nothing: const data lives in flash */
#define LV_FONT_SUBPX_NONE        0

/* Character-map types. */
typedef enum {
    LV_FONT_FMT_TXT_CMAP_FORMAT0_FULL,   /* range, with a glyph-offset table   */
    LV_FONT_FMT_TXT_CMAP_SPARSE_FULL,    /* list of code points + offsets      */
    LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY,   /* range, glyphs numbered in order    */
    LV_FONT_FMT_TXT_CMAP_SPARSE_TINY,    /* list of code points, in order      */
} lv_font_fmt_txt_cmap_type_t;

/* One glyph's description. Bit-fields keep it to 8 bytes per glyph. */
typedef struct {
    uint32_t bitmap_index : 20;   /* byte offset into glyph_bitmap[]        */
    uint32_t adv_w        : 12;   /* advance, in 1/16 pixel                 */
    uint8_t  box_w;               /* glyph box width,  pixels               */
    uint8_t  box_h;               /* glyph box height, pixels               */
    int8_t   ofs_x;               /* box left edge, right of the pen        */
    int8_t   ofs_y;               /* box bottom edge, above the baseline    */
} lv_font_fmt_txt_glyph_dsc_t;

/* One character map entry (a font can have several). */
typedef struct {
    uint32_t                    range_start;       /* first code point        */
    uint16_t                    range_length;      /* how many code points    */
    uint16_t                    glyph_id_start;    /* glyph number of the 1st */
    const void                 *unicode_list;      /* SPARSE: uint16 offsets  */
    const void                 *glyph_id_ofs_list; /* FULL: glyph offsets     */
    uint16_t                    list_length;
    lv_font_fmt_txt_cmap_type_t type;
} lv_font_fmt_txt_cmap_t;

/* Kerning, form 1: a sorted list of glyph pairs and their adjustments. */
typedef struct {
    const void   *glyph_ids;          /* pairs: [left, right, left, right...] */
    const int8_t *values;             /* one adjustment per pair              */
    uint32_t      pair_cnt       : 30;
    uint32_t      glyph_ids_size : 2; /* 0 = uint8 ids, 1 = uint16 ids       */
} lv_font_fmt_txt_kern_pair_t;

/* Kerning, form 2: glyphs grouped into classes, with a class-pair table. */
typedef struct {
    const int8_t  *class_pair_values; /* [left_class-1][right_class-1]        */
    const uint8_t *left_class_mapping;  /* glyph -> left class (0 = none)     */
    const uint8_t *right_class_mapping; /* glyph -> right class (0 = none)    */
    uint8_t        left_class_cnt;
    uint8_t        right_class_cnt;
} lv_font_fmt_txt_kern_classes_t;

/* LVGL 8 files declare a small lookup cache; we don't use it. */
typedef struct {
    uint32_t last_letter;
    uint32_t last_glyph_id;
} lv_font_fmt_txt_glyph_cache_t;

/* Everything about one font's tables. */
typedef struct {
    const uint8_t                     *glyph_bitmap;
    const lv_font_fmt_txt_glyph_dsc_t *glyph_dsc;
    const lv_font_fmt_txt_cmap_t      *cmaps;
    const void                        *kern_dsc;     /* pairs or classes     */
    uint16_t                           kern_scale;   /* x/16 applied to kerns */
    uint16_t                           cmap_num;
    uint8_t                            bpp;          /* bits per pixel       */
    uint8_t                            kern_classes; /* 1 = kern_dsc is classes */
    uint8_t                            bitmap_format;/* 0 = plain            */
    lv_font_fmt_txt_glyph_cache_t     *cache;
} lv_font_fmt_txt_dsc_t;

/* The public font object a font file defines. */
typedef struct _lv_font_t {
    /* LVGL uses these two function pointers; this library reads the tables
     * directly and never calls them. Stubs exist only so files link.        */
    bool (*get_glyph_dsc)(const struct _lv_font_t *font, void *dsc_out,
                          uint32_t letter, uint32_t letter_next);
    const uint8_t *(*get_glyph_bitmap)(const struct _lv_font_t *font, uint32_t letter);

    int32_t     line_height;          /* pixels, one full line              */
    int32_t     base_line;            /* baseline, up from the line bottom  */
    uint8_t     subpx;
    int8_t      underline_position;
    int8_t      underline_thickness;
    const void *dsc;                  /* -> lv_font_fmt_txt_dsc_t           */
    const struct _lv_font_t *fallback;
    void       *user_data;
} lv_font_t;

/* The functions LVGL font files point at (never called; see above). */
bool lv_font_get_glyph_dsc_fmt_txt(const lv_font_t *font, void *dsc_out,
                                   uint32_t letter, uint32_t letter_next);
const uint8_t *lv_font_get_bitmap_fmt_txt(const lv_font_t *font, uint32_t letter);

/* Declare a font defined in another file:  LV_FONT_DECLARE(qg_font_sans_16); */
#define LV_FONT_DECLARE(name) extern const lv_font_t name;

#endif /* QG_FONT_FORMAT_H */
