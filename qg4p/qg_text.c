/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    qg_text.c
 * @brief   The text engine: finding glyphs, kerning, and drawing characters.
 *
 * LAYER:   Public API (text)
 * DEPENDS: qg_text.h, qg_internal.h, qg_draw_pct.h
 *
 * THE FILE, BOTTOM-UP
 *   Glyph level  UTF-8 decoding, character -> glyph lookup, kerning, and
 *                drawing one glyph (transparent or opaque).
 *   Engine       tokens (characters, escapes, markup), line layout with word
 *                wrap, drawing a laid-out line, and scrolling memory.
 *   Public API   qg_print and friends, at the end.
 */
#include <stdio.h>
#include <string.h>
#include "qg_text.h"
#include "qg_internal.h"
#include "qg_draw_pct.h"
#include "qg_palette.h"

/* ========================================================================== */
/*  Stubs for LVGL font files                                                 */
/* ========================================================================== */

/* Font files store pointers to these two LVGL functions. This library reads
 * the tables directly and never calls them; they exist only so that font
 * files link.                                                               */
bool lv_font_get_glyph_dsc_fmt_txt(const lv_font_t *font, void *dsc_out,
                                   uint32_t letter, uint32_t letter_next)
{
    (void)font; (void)dsc_out; (void)letter; (void)letter_next;
    return false;
}

const uint8_t *lv_font_get_bitmap_fmt_txt(const lv_font_t *font, uint32_t letter)
{
    (void)font; (void)letter;
    return NULL;
}

/* ========================================================================== */
/*  Font objects                                                              */
/* ========================================================================== */

static inline const lv_font_fmt_txt_dsc_t *tables(const qg_font_t *font)
{
    return (const lv_font_fmt_txt_dsc_t *)font->data->dsc;
}

/** True if a colour would actually put pixels on the screen. */
static inline bool is_color(qg_color_t c)
{
    return c <= 254;
}

static inline int32_t clamp_scale(uint8_t s)
{
    return (s < 1) ? 1 : (s > 4) ? 4 : s;
}

qg_font_t qg_font_create(const lv_font_t *data, qg_color_t color, uint8_t scale)
{
    qg_font_t f = { .data = data, .color = color, .scale = (uint8_t)clamp_scale(scale) };
    return f;
}

qg_err_t qg_font_check(const lv_font_t *data)
{
    if (data == NULL || data->dsc == NULL) {
        return QG_ERR_ARG;
    }
    const lv_font_fmt_txt_dsc_t *d = (const lv_font_fmt_txt_dsc_t *)data->dsc;
    if (d->glyph_bitmap == NULL || d->glyph_dsc == NULL || d->cmaps == NULL) {
        return QG_ERR_ARG;
    }
    if (d->bpp != 1 || d->bitmap_format != 0) {
        return QG_ERR_UNSUPPORTED;   /* v1: 1 bpp, uncompressed only */
    }
    return QG_OK;
}

int16_t qg_font_line_height(const qg_font_t *font)
{
    if (font == NULL || font->data == NULL) return 0;
    return (int16_t)(font->data->line_height * clamp_scale(font->scale));
}

qg_err_t qg_screen_set_font(qg_screen_t *scr, uint8_t slot, const qg_font_t *font)
{
    if (scr == NULL || slot >= QG_MAX_FONTS) {
        return QG_ERR_ARG;
    }
    if (font != NULL) {
        qg_err_t err = qg_font_check(font->data);
        if (err != QG_OK) return err;
    }
    scr->fonts[slot] = font;
    return QG_OK;
}

void qg_screen_set_text_bg(qg_screen_t *scr, qg_color_t bg)
{
    if (scr == NULL) return;
    scr->text_bg = (bg <= 254) ? bg : QG_TRANSPARENT;
}

/* ========================================================================== */
/*  UTF-8                                                                     */
/* ========================================================================== */

/*
 * UTF-8 stores ASCII as single bytes and every other character as 2 to 4
 * bytes. The first byte's top bits say how many bytes follow:
 *
 *   0xxxxxxx                              1 byte   (ASCII)
 *   110xxxxx 10xxxxxx                     2 bytes  (e.g. "°" = C2 B0)
 *   1110xxxx 10xxxxxx 10xxxxxx            3 bytes  (e.g. "€")
 *   11110xxx 10xxxxxx 10xxxxxx 10xxxxxx   4 bytes
 *
 * The x bits, joined together, are the character's code point. Malformed
 * input becomes U+FFFD, the "replacement character", which prints as "?".
 */
static uint32_t utf8_next(const char **p)
{
    const uint8_t *s = (const uint8_t *)*p;
    uint32_t c = s[0];
    int extra;

    if (c == 0)             { return 0; }
    if (c < 0x80)           { *p += 1; return c; }
    if ((c & 0xE0) == 0xC0) { c &= 0x1F; extra = 1; }
    else if ((c & 0xF0) == 0xE0) { c &= 0x0F; extra = 2; }
    else if ((c & 0xF8) == 0xF0) { c &= 0x07; extra = 3; }
    else                    { *p += 1; return 0xFFFD; }

    for (int i = 1; i <= extra; i++) {
        if ((s[i] & 0xC0) != 0x80) {       /* truncated or broken sequence */
            *p += i;
            return 0xFFFD;
        }
        c = (c << 6) | (s[i] & 0x3F);
    }
    *p += 1 + extra;
    return c;
}

/* ========================================================================== */
/*  Glyph lookup and kerning                                                  */
/* ========================================================================== */

/** Binary search for `key` in a sorted uint16_t list. Returns index or -1. */
static int32_t find_u16(const uint16_t *list, uint32_t len, uint32_t key)
{
    uint32_t lo = 0, hi = len;
    while (lo < hi) {
        uint32_t mid = (lo + hi) / 2;
        if (list[mid] < key)      lo = mid + 1;
        else if (list[mid] > key) hi = mid;
        else                      return (int32_t)mid;
    }
    return -1;
}

/*
 * CHARACTER -> GLYPH NUMBER
 * A font has one or more character maps ("cmaps"). Each covers a range of
 * code points starting at range_start. Four kinds exist:
 *   FORMAT0_TINY  every code point in the range has a glyph, numbered in
 *                 order: glyph = glyph_id_start + offset
 *   FORMAT0_FULL  as above, but a table gives each glyph's number
 *   SPARSE_TINY   only the code points in a sorted list have glyphs,
 *                 numbered in list order
 *   SPARSE_FULL   a sorted list plus a table of glyph numbers
 * Glyph 0 means "not in this font".
 */
static uint32_t glyph_id(const lv_font_fmt_txt_dsc_t *d, uint32_t cp)
{
    for (uint32_t i = 0; i < d->cmap_num; i++) {
        const lv_font_fmt_txt_cmap_t *c = &d->cmaps[i];
        if (cp < c->range_start) continue;
        uint32_t ofs = cp - c->range_start;
        if (ofs >= c->range_length) continue;

        switch (c->type) {
        case LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY:
            return c->glyph_id_start + ofs;

        case LV_FONT_FMT_TXT_CMAP_FORMAT0_FULL:
            return c->glyph_id_start + ((const uint8_t *)c->glyph_id_ofs_list)[ofs];

        case LV_FONT_FMT_TXT_CMAP_SPARSE_TINY: {
            int32_t k = find_u16((const uint16_t *)c->unicode_list, c->list_length, ofs);
            if (k >= 0) return c->glyph_id_start + (uint32_t)k;
            break;
        }
        case LV_FONT_FMT_TXT_CMAP_SPARSE_FULL: {
            int32_t k = find_u16((const uint16_t *)c->unicode_list, c->list_length, ofs);
            if (k >= 0) {
                return c->glyph_id_start + ((const uint16_t *)c->glyph_id_ofs_list)[k];
            }
            break;
        }
        }
    }
    return 0;
}

/*
 * KERNING: how much to nudge glyph `right` when it follows glyph `left`.
 * Returned in 1/16 pixel (negative = closer together).
 *
 *   Pair form:  a sorted list of (left, right) glyph pairs, each with its own
 *               value. Found by binary search.
 *   Class form: every glyph belongs to a left class and a right class (0 =
 *               none); a table holds one value per class pair. This is more
 *               compact when many letters share the same spacing, like all
 *               the round letters o, c, e.
 */
static int32_t kerning(const lv_font_fmt_txt_dsc_t *d, uint32_t left, uint32_t right)
{
    if (d->kern_dsc == NULL || left == 0 || right == 0) {
        return 0;
    }
    int32_t value = 0;

    if (d->kern_classes == 0) {
        const lv_font_fmt_txt_kern_pair_t *k = (const lv_font_fmt_txt_kern_pair_t *)d->kern_dsc;
        uint32_t lo = 0, hi = k->pair_cnt;
        while (lo < hi) {
            uint32_t mid = (lo + hi) / 2, l, r;
            if (k->glyph_ids_size == 0) {
                const uint8_t *ids = (const uint8_t *)k->glyph_ids;
                l = ids[mid * 2]; r = ids[mid * 2 + 1];
            } else {
                const uint16_t *ids = (const uint16_t *)k->glyph_ids;
                l = ids[mid * 2]; r = ids[mid * 2 + 1];
            }
            if (l < left || (l == left && r < right))      lo = mid + 1;
            else if (l > left || (l == left && r > right)) hi = mid;
            else { value = k->values[mid]; break; }
        }
    } else {
        const lv_font_fmt_txt_kern_classes_t *k = (const lv_font_fmt_txt_kern_classes_t *)d->kern_dsc;
        uint8_t lc = k->left_class_mapping[left];
        uint8_t rc = k->right_class_mapping[right];
        if (lc > 0 && rc > 0) {
            value = k->class_pair_values[(lc - 1) * k->right_class_cnt + (rc - 1)];
        }
    }
    /* The stored value is scaled by kern_scale/16 (this is how LVGL fonts
     * pack a wide range of adjustments into one signed byte).              */
    return (value * (int32_t)d->kern_scale) >> 4;
}

/* ========================================================================== */
/*  Drawing one glyph                                                         */
/* ========================================================================== */

/* Bit n of a glyph's packed bitmap (MSB first, rows run on without padding). */
static inline bool bit_at(const uint8_t *bmp, uint32_t n)
{
    return (bmp[n >> 3] >> (7 - (n & 7))) & 1;
}

static bool rows_equal(const uint8_t *bmp, uint32_t w, uint32_t r1, uint32_t r2)
{
    for (uint32_t x = 0; x < w; x++) {
        if (bit_at(bmp, r1 * w + x) != bit_at(bmp, r2 * w + x)) return false;
    }
    return true;
}

/*
 * TRANSPARENT GLYPHS: only the "on" pixels are drawn, so whatever is behind
 * the text shows through.
 *
 * Each row is split into runs of set pixels, and each run is sent as one
 * rectangle (scaled up if needed). Identical neighbouring rows are merged:
 * the stem of an 'l' or '|' is one tall rectangle instead of a dozen short
 * ones, which matters on a DIRECT screen where every rectangle costs a small
 * amount of bus setup.
 */
static void glyph_transparent(qg_screen_t *scr, const uint8_t *bmp,
                              int32_t w, int32_t h, int32_t gx, int32_t gy,
                              int32_t scale, qg_color_t color)
{
    int32_t row = 0;
    while (row < h) {
        int32_t same = 1;
        while (row + same < h && rows_equal(bmp, (uint32_t)w, (uint32_t)row,
                                            (uint32_t)(row + same))) {
            same++;
        }

        int32_t run = -1;
        for (int32_t x = 0; x <= w; x++) {
            bool on = (x < w) && bit_at(bmp, (uint32_t)(row * w + x));
            if (on && run < 0) {
                run = x;
            } else if (!on && run >= 0) {
                qg_int_fill_rect(scr, gx + run * scale, gy + row * scale,
                                  (x - run) * scale, same * scale, color);
                run = -1;
            }
        }
        row += same;
    }
}

/*
 * OPAQUE GLYPHS: the whole character cell is painted, background included.
 *
 * FAST PATH: build the cell in a RAM buffer (background colour everywhere,
 * then the glyph's pixels) and send it as a single block. One bus
 * transaction per character, and nothing ever flickers, because each pixel
 * is sent exactly once in its final colour.
 *
 * SLOW PATH, for cells that are partly off-screen or bigger than the buffer:
 * fill the cell with the background, then draw the glyph on top.
 */
#if QG_TEXT_CELL_PIXELS > 0
static uint16_t s_cell[QG_TEXT_CELL_PIXELS];   /* QG_TEXT_CELL_PIXELS 0 removes it */
#endif

static void glyph_opaque(qg_screen_t *scr, const uint8_t *bmp,
                         int32_t w, int32_t h, int32_t gx, int32_t gy,
                         int32_t cell_x, int32_t cell_y, int32_t cell_w, int32_t cell_h,
                         int32_t scale, qg_color_t fg, qg_color_t bg)
{
    /* Fast path only when the whole cell is inside the view (checked in
     * screen pixels: the cell's coordinates are relative to the origin).   */
    int32_t sx = cell_x + scr->origin_x, sy = cell_y + scr->origin_y;
    bool on_screen = sx >= scr->view_x0 && sy >= scr->view_y0 &&
                     sx + cell_w <= scr->view_x1 && sy + cell_h <= scr->view_y1;

#if QG_TEXT_CELL_PIXELS > 0
    if (!on_screen || cell_w * cell_h > QG_TEXT_CELL_PIXELS ||
        scr->backend->write_rgb565 == NULL)
#endif
    {
        qg_int_fill_rect(scr, cell_x, cell_y, cell_w, cell_h, bg);
        glyph_transparent(scr, bmp, w, h, gx, gy, scale, fg);
        return;
    }
#if QG_TEXT_CELL_PIXELS > 0

    uint16_t bg565 = qg_int_out(scr)[bg];              /* as sent to the panel */
    uint16_t fg565 = qg_int_out(scr)[fg];
    for (int32_t i = 0; i < cell_w * cell_h; i++) {
        s_cell[i] = bg565;
    }

    /* Copy the glyph into the cell. Parts of the glyph outside the cell
     * (possible with italics or very wide letters) are cut off.            */
    for (int32_t row = 0; row < h; row++) {
        for (int32_t col = 0; col < w; col++) {
            if (!bit_at(bmp, (uint32_t)(row * w + col))) continue;
            for (int32_t ry = 0; ry < scale; ry++) {           /* scaled copies */
                int32_t py = gy + row * scale + ry - cell_y;
                if (py < 0 || py >= cell_h) continue;
                for (int32_t rx = 0; rx < scale; rx++) {
                    int32_t px = gx + col * scale + rx - cell_x;
                    if (px < 0 || px >= cell_w) continue;
                    s_cell[py * cell_w + px] = fg565;
                }
            }
        }
    }
    scr->backend->write_rgb565(scr, (int16_t)sx, (int16_t)sy,
                               (int16_t)cell_w, (int16_t)cell_h, s_cell);
#else
    (void)on_screen;
#endif
}

/* ========================================================================== */
/*  The text engine                                                           */
/* ========================================================================== */
/*
 * HOW A PRINT CALL WORKS
 *
 *   text ──► TOKENS ──► LAYOUT (one line at a time) ──► DRAW
 *
 *   Tokens    the string read piece by piece: a character, a newline, a tab,
 *             or a markup tag that changes the style.
 *
 *   Layout    reads ahead to find where the current line ends (a "\n", the
 *             end of the text, or a word that won't fit) and how tall it is.
 *             This has to happen BEFORE drawing: if a line mixes a small and
 *             a large font, every character must sit on one shared baseline,
 *             so the tallest font on the line has to be known first.
 *
 *   Draw      reads the same tokens again, from the line's start to its end,
 *             and draws them on that baseline. (Measuring text is the same
 *             process with drawing switched off.)
 *
 * The pen position is kept in 1/16 pixel, the same unit fonts use for
 * advance widths, so fractional widths add up correctly across a line.
 */

/* --- Style: what markup can change ---------------------------------------- */

typedef struct {
    const qg_font_t *font;        /* current font                              */
    int8_t            mscale;      /* {s:n} scale, or 0 = use the font's own    */
    int16_t           mcolor;      /* {c:x} colour, or -1 = none                */
    qg_color_t       base_color;  /* the call's colour (print_at), or DEFAULT  */
    qg_color_t       fg;          /* screen foreground, the final fallback     */
} style_t;

static inline int32_t st_scale(const style_t *st)
{
    return st->mscale ? st->mscale : clamp_scale(st->font->scale);
}

/* Text colour: markup, else the call's colour, else the font's, else fg. */
static inline qg_color_t st_color(const style_t *st)
{
    if (st->mcolor >= 0)                return (qg_color_t)st->mcolor;
    if (st->base_color != QG_DEFAULT)  return st->base_color;
    if (st->font->color != QG_DEFAULT) return st->font->color;
    return st->fg;
}

/* Height above and below the baseline for the current style. */
static inline int32_t st_ascent(const style_t *st)
{
    return (st->font->data->line_height - st->font->data->base_line) * st_scale(st);
}
static inline int32_t st_descent(const style_t *st)
{
    return st->font->data->base_line * st_scale(st);
}

/* --- Everything one print call needs --------------------------------------- */

typedef struct {
    qg_screen_t     *scr;
    const qg_font_t *base_font;   /* what {f:} returns to                      */
    int32_t           left;        /* margin: where new lines start (px)        */
    int32_t           right;       /* wrap when text passes this x (px)         */
    bool              wrap;
    qg_align_t       align;
} ctx_t;

/* --- Tokens ------------------------------------------------------------------ */

typedef enum { T_END, T_CHAR, T_NEWLINE, T_CR, T_TAB, T_MOVE_X, T_STYLE } tok_t;

/* Parse a whole number from s[0..len). Returns false if it isn't one. */
static bool parse_int(const char *s, size_t len, int32_t *out)
{
    if (len == 0 || len > 6) return false;
    int32_t v = 0;
    bool neg = false;
    size_t i = 0;
    if (s[0] == '-') { neg = true; i = 1; if (len == 1) return false; }
    for (; i < len; i++) {
        if (s[i] < '0' || s[i] > '9') return false;
        v = v * 10 + (s[i] - '0');
    }
    *out = neg ? -v : v;
    return true;
}

/*
 * Apply one markup tag. `body` points just after the "{" and `len` is the
 * length up to (not including) the "}". Returns T_MOVE_X for {x:n} (with the
 * column in *x), otherwise T_STYLE. Unknown or malformed tags change nothing.
 */
static tok_t apply_tag(const ctx_t *c, style_t *st, const char *body, size_t len, int32_t *x)
{
    /* The internal {~:...} tag used by scrolling (see history_add). It
     * restores the exact state a remembered line started with.             */
    if (len >= 2 && body[0] == '~' && body[1] == ':') {
        int32_t v[6] = { 0, -1, (int32_t)c->scr->fg_color, 0, 0, -1 };
        const char *q = body + 2, *end = body + len;
        for (int i = 0; i < 6 && q < end; i++) {
            const char *comma = q;
            while (comma < end && *comma != ',') comma++;
            parse_int(q, (size_t)(comma - q), &v[i]);
            q = comma + 1;
        }
        /* x, base colour, fg, font slot, markup scale, markup colour */
        st->base_color = (v[1] >= 0) ? (qg_color_t)v[1] : QG_DEFAULT;
        st->fg         = (qg_color_t)v[2];
        if (v[3] >= 0 && v[3] < QG_MAX_FONTS && c->scr->fonts[v[3]] != NULL) {
            st->font = c->scr->fonts[v[3]];
        }
        st->mscale = (int8_t)v[4];
        st->mcolor = (int16_t)v[5];
        *x = v[0];
        return T_MOVE_X;
    }
    if (len < 2 || body[1] != ':') {
        return T_STYLE;                          /* not key:value: ignored   */
    }

    const char  key = body[0];
    const char *val = body + 2;
    size_t      vlen = len - 2;
    int32_t     n;

    switch (key) {
    case 'c':                                    /* colour                   */
        if (vlen == 0) {
            st->mcolor = -1;
        } else if (parse_int(val, vlen, &n)) {
            if (n >= 0 && n <= 254) st->mcolor = (int16_t)n;
        } else {
            int named = qg_color_from_name(val, vlen);
            if (named >= 0) st->mcolor = (int16_t)named;
        }
        break;
    case 'f':                                    /* font slot                */
        if (vlen == 0) {
            st->font = c->base_font;
        } else if (parse_int(val, vlen, &n) && n >= 0 && n < QG_MAX_FONTS &&
                   c->scr->fonts[n] != NULL) {
            st->font = c->scr->fonts[n];
        }
        break;
    case 's':                                    /* scale                    */
        if (vlen == 0) {
            st->mscale = 0;
        } else if (parse_int(val, vlen, &n) && n >= 1 && n <= 4) {
            st->mscale = (int8_t)n;
        }
        break;
    case 'x':                                    /* move to column           */
        if (parse_int(val, vlen, &n)) {
            *x = n;
            return T_MOVE_X;
        }
        break;
    default:
        break;                                   /* unknown: ignored         */
    }
    return T_STYLE;
}

/*
 * Read the next token from *p and step past it.
 *   T_CHAR:   *cp holds the character      T_MOVE_X: *x holds the column
 *   T_STYLE:  the style in *st was updated
 */
#define TAG_MAX 32   /* a "{" with no "}" within this many chars is literal */

static tok_t next_token(const ctx_t *c, const char **p, style_t *st,
                        uint32_t *cp, int32_t *x)
{
    const char *s = *p;
    if (*s == '\0') return T_END;
    if (*s == '\n') { *p = s + 1; return T_NEWLINE; }
    if (*s == '\r') { *p = s + 1; return T_CR; }
    if (*s == '\t') { *p = s + 1; return T_TAB; }

    if (*s == '{') {
        if (s[1] == '{') {                       /* "{{" -> a literal "{"   */
            *p = s + 2;
            *cp = '{';
            return T_CHAR;
        }
        const char *close = s + 1;
        while (*close != '\0' && *close != '}' && close - s < TAG_MAX) close++;
        if (*close == '}') {
            *p = close + 1;
            return apply_tag(c, st, s + 1, (size_t)(close - s - 1), x);
        }
        /* No closing brace: fall through and print the "{" itself. */
    }

    *cp = utf8_next(p);
    if (*cp == 0) return T_END;
    return T_CHAR;
}

/* Where the next tab stop after `pen16` is, counting from the margin. */
static int32_t next_tab16(const ctx_t *c, int32_t pen16)
{
    int32_t tw16 = (c->scr->tab_width > 0 ? c->scr->tab_width : 1) * 16;
    int32_t rel  = pen16 - c->left * 16;
    if (rel < 0) return c->left * 16;
    return c->left * 16 + (rel / tw16 + 1) * tw16;
}

/* --- Layout ------------------------------------------------------------------ */

typedef struct {
    const char *start;        /* first token of the line                       */
    const char *end;          /* just past the line's last token               */
    const char *next;         /* where the next line starts (NULL = no more)   */
    style_t     st_start;     /* style at `start`                              */
    style_t     st_end;       /* style at `next`                               */
    int32_t     pen16_start;  /* pen at `start`                                */
    int32_t     pen16_end;    /* pen after the line's last token (incl. spaces)*/
    int32_t     width;        /* from the margin to the last visible glyph, px */
    int32_t     ascent;       /* tallest part above the baseline               */
    int32_t     descent;      /* deepest part below it                         */
} line_t;

/*
 * Find where one line ends and how big it is.
 *
 * WORD WRAP
 * While reading, the last place a line could break is remembered: just
 * after a run of spaces, i.e. at the start of a word. When a character
 * would pass the right edge:
 *   - if there is such a place, the line ends there, and the word that
 *     didn't fit starts the next line;
 *   - if not (one word longer than the whole line), the line ends just
 *     before the character that didn't fit, splitting the word.
 * Everything measured (width, height, style) is snapshotted at the break
 * point, so the line's figures cover only what actually stays on it.
 */
static void layout_line(const ctx_t *c, const char *p, style_t st, int32_t pen16,
                        line_t *ln)
{
    ln->start       = p;
    ln->st_start    = st;
    ln->pen16_start = pen16;

    int32_t  asc = st_ascent(&st), desc = st_descent(&st);
    int32_t  vis16 = pen16;           /* pen at the end of the last non-space */
    bool     content = false;         /* anything visible on this line yet?   */
    uint32_t prev = 0;
    const qg_font_t *prev_font = st.font;

    /* The last good break point (start of a word after spaces). */
    bool        have_brk = false;
    const char *brk_p = NULL;
    style_t     brk_st = st;
    int32_t     brk_vis16 = 0, brk_asc = 0, brk_desc = 0;
    bool        in_space = false;

    for (;;) {
        const char *before = p;
        style_t     st_before = st;
        uint32_t    cp = 0;
        int32_t     xv = 0;
        tok_t       t = next_token(c, &p, &st, &cp, &xv);

        if (t == T_END) {
            ln->end = before; ln->next = NULL; ln->st_end = st; ln->pen16_end = pen16;
            break;
        }
        if (t == T_NEWLINE) {
            ln->end = before; ln->next = p; ln->st_end = st; ln->pen16_end = pen16;
            break;
        }
        if (t == T_STYLE) {
            if (st.font != prev_font) { prev = 0; prev_font = st.font; }
            continue;
        }
        if (t == T_CR)     { pen16 = c->left * 16; prev = 0; continue; }
        if (t == T_MOVE_X) { pen16 = xv * 16;      prev = 0; continue; }
        if (t == T_TAB) {
            pen16 = next_tab16(c, pen16);
            prev = 0;
            if (content && !in_space) {          /* a tab also ends a word */
                in_space = true;
            }
            continue;
        }

        /* --- a character --- */
        if (cp < 0x20) continue;
        const lv_font_fmt_txt_dsc_t *d = tables(st.font);
        uint32_t gid = glyph_id(d, cp);
        if (gid == 0) gid = glyph_id(d, '?');
        if (gid == 0) continue;

        int32_t scale = st_scale(&st);
        int32_t k16   = kerning(d, prev, gid) * scale;
        int32_t adv16 = (int32_t)d->glyph_dsc[gid].adv_w * scale;
        bool    space = (cp == ' ');

        /* A space after visible text marks the end of a word. */
        if (space) {
            in_space = content ? true : in_space;
            pen16 += k16 + adv16;
            prev = gid;
            continue;
        }
        /* First character of a new word: remember this as a break point. */
        if (in_space) {
            have_brk  = true;
            brk_p     = before;
            brk_st    = st_before;
            brk_vis16 = vis16;
            brk_asc   = asc;
            brk_desc  = desc;
            in_space  = false;
        }

        /* Would this character pass the right edge? */
        if (c->wrap && content && pen16 + k16 + adv16 > c->right * 16) {
            if (have_brk) {
                ln->end = brk_p; ln->next = brk_p; ln->st_end = brk_st;
                vis16 = brk_vis16; asc = brk_asc; desc = brk_desc;
            } else {
                ln->end = before; ln->next = before; ln->st_end = st_before;
            }
            ln->pen16_end = vis16;
            break;
        }

        pen16 += k16 + adv16;
        vis16  = pen16;
        prev   = gid;
        content = true;
        if (st_ascent(&st)  > asc)  asc  = st_ascent(&st);
        if (st_descent(&st) > desc) desc = st_descent(&st);
    }

    ln->width   = (vis16 + 8) / 16 - c->left;
    ln->ascent  = asc;
    ln->descent = desc;
}

/* --- Drawing one laid-out line ----------------------------------------------- */

/* Draw the line from ln->start to ln->end with its top at `top`, shifted
 * right by `shift` pixels (for alignment). */
static void draw_line(const ctx_t *c, const line_t *ln, int32_t top, int32_t shift)
{
    qg_screen_t *scr     = c->scr;
    const char   *p       = ln->start;
    style_t       st      = ln->st_start;
    int32_t       pen16   = ln->pen16_start + shift * 16;
    int32_t       base_y  = top + ln->ascent;             /* the baseline      */
    int32_t       line_h  = ln->ascent + ln->descent;
    bool          opaque  = is_color(scr->text_bg);
    uint32_t      prev    = 0;
    const qg_font_t *prev_font = st.font;

    while (p < ln->end) {
        uint32_t cp = 0;
        int32_t  xv = 0;
        int32_t  pen_before = pen16;
        tok_t    t = next_token(c, &p, &st, &cp, &xv);

        if (t == T_END || t == T_NEWLINE) break;
        if (t == T_STYLE) {
            if (st.font != prev_font) { prev = 0; prev_font = st.font; }
            continue;
        }
        if (t == T_CR)     { pen16 = (c->left + shift) * 16; prev = 0; continue; }
        if (t == T_MOVE_X) { pen16 = xv * 16; prev = 0; continue; }
        if (t == T_TAB) {
            pen16 = next_tab16(c, pen16 - shift * 16) + shift * 16;
            prev = 0;
            if (opaque) {                     /* paint the gap as background */
                int32_t x0 = (pen_before + 8) / 16, x1 = (pen16 + 8) / 16;
                qg_int_fill_rect(scr, x0, top, x1 - x0, line_h, scr->text_bg);
            }
            continue;
        }

        if (cp < 0x20) continue;
        const lv_font_fmt_txt_dsc_t *d = tables(st.font);
        uint32_t gid = glyph_id(d, cp);
        if (gid == 0) gid = glyph_id(d, '?');
        if (gid == 0) continue;

        int32_t scale = st_scale(&st);
        pen16 += kerning(d, prev, gid) * scale;
        prev = gid;

        const lv_font_fmt_txt_glyph_dsc_t *g = &d->glyph_dsc[gid];
        int32_t adv16 = (int32_t)g->adv_w * scale;
        int32_t pen_x = (pen16 + 8) / 16;
        qg_color_t fg = st_color(&st);

        /* The glyph box's bottom edge sits ofs_y above the baseline. */
        int32_t gx = pen_x + g->ofs_x * scale;
        int32_t gy = base_y - (g->box_h + g->ofs_y) * scale;
        const uint8_t *bmp = &d->glyph_bitmap[g->bitmap_index];

        if (opaque) {
            int32_t cell_w = (pen16 + adv16 + 8) / 16 - pen_x;
            glyph_opaque(scr, bmp, g->box_w, g->box_h, gx, gy,
                         pen_x, top, cell_w, line_h, scale, fg, scr->text_bg);
        } else if (g->box_w > 0 && is_color(fg)) {
            glyph_transparent(scr, bmp, g->box_w, g->box_h, gx, gy, scale, fg);
        }
        pen16 += adv16;
    }
}

/* ========================================================================== */
/*  Scrolling memory (DIRECT screens)                                         */
/* ========================================================================== */
/*
 * A DIRECT screen can't move its own pixels, so to scroll we clear it and
 * reprint what was there, one row higher. For that, every line printed by
 * qg_print/qg_println is remembered as a short markup string.
 *
 * A line can be built from several print calls, each with its own starting
 * style ("Score: " in grey, then "17" in green). So each piece is stored with
 * an internal tag in front that restores its exact starting state:
 *
 *     {~:x,base,fg,slot,scale,colour}text...
 *
 * Reprinting the stored string then reproduces the line exactly, as long as
 * the fonts in the screen's slots haven't changed in between.
 */
static int slot_of(const qg_screen_t *scr, const qg_font_t *f)
{
    for (int i = 0; i < QG_MAX_FONTS; i++) {
        if (scr->fonts[i] == f) return i;
    }
    return 0;
}

/* Append n bytes to a remembered line, truncating if it's full. */
static void hist_append(qg_text_line_t *h, const char *s, size_t n)
{
    size_t room = sizeof(h->text) - 1 - h->len;
    if (n > room) n = room;
    memcpy(h->text + h->len, s, n);
    h->len = (uint16_t)(h->len + n);
    h->text[h->len] = '\0';
}

static void history_add(qg_screen_t *scr, const line_t *ln, int32_t top, int32_t line_h)
{
    qg_text_line_t *h;
    if (scr->hist_cap == 0) return;          /* no memory given: nothing to keep */

    bool same_line = scr->hist_open && scr->hist_count > 0 &&
                     scr->hist[scr->hist_count - 1].y == top;
    if (same_line) {
        h = &scr->hist[scr->hist_count - 1];
        if (line_h > h->h) h->h = (int16_t)line_h;
    } else {
        if (scr->hist_count == scr->hist_cap) {
            /* Full: forget the oldest line. */
            memmove(&scr->hist[0], &scr->hist[1],
                    sizeof(scr->hist[0]) * (size_t)(scr->hist_cap - 1));
            scr->hist_count--;
        }
        h = &scr->hist[scr->hist_count++];
        h->y = (int16_t)top;
        h->h = (int16_t)line_h;
        h->len = 0;
        h->text[0] = '\0';
    }

    char tag[48];
    const style_t *st = &ln->st_start;
    int n = snprintf(tag, sizeof tag, "{~:%ld,%d,%d,%d,%d,%d}",
                     (long)((ln->pen16_start + 8) / 16),
                     st->base_color == QG_DEFAULT ? -1 : (int)st->base_color,
                     (int)st->fg, slot_of(scr, st->font), (int)st->mscale, (int)st->mcolor);
    if (n > 0) hist_append(h, tag, (size_t)n);
    hist_append(h, ln->start, (size_t)(ln->end - ln->start));
    scr->hist_open = true;
}

/* Scroll the screen up by `dy` pixels by clearing it and reprinting. */
static void scroll_up(qg_screen_t *scr, int32_t dy)
{
    int keep = 0;
    for (int i = 0; i < scr->hist_count; i++) {
        qg_text_line_t *h = &scr->hist[i];
        h->y = (int16_t)(h->y - dy);
        if (h->y + h->h > 0) {                 /* still at least partly visible */
            if (keep != i) scr->hist[keep] = *h;
            keep++;
        }
    }
    scr->hist_count = (uint8_t)keep;

    qg_int_fill_rect(scr, 0, 0, scr->width, scr->height, scr->bg_color);

    ctx_t c = { .scr = scr, .base_font = scr->fonts[0], .left = 0,
                .right = scr->width, .wrap = false, .align = QG_ALIGN_LEFT };
    for (int i = 0; i < scr->hist_count; i++) {
        style_t st = { .font = scr->fonts[0], .mscale = 0, .mcolor = -1,
                       .base_color = QG_DEFAULT, .fg = scr->fg_color };
        line_t ln;
        layout_line(&c, scr->hist[i].text, st, 0, &ln);
        /* Keep the line's original height so the rows stay evenly spaced. */
        int32_t extra = scr->hist[i].h - (ln.ascent + ln.descent);
        if (extra > 0) ln.ascent += extra;
        draw_line(&c, &ln, scr->hist[i].y, 0);
    }
}


/* ========================================================================== */
/*  Running a whole print call                                                */
/* ========================================================================== */

typedef struct {
    int32_t end_x, end_y;     /* where the next character would go            */
    int32_t last_h;           /* height of the last line                      */
    int32_t max_w;            /* widest line                                  */
    int32_t total_h;          /* all lines                                    */
} run_result_t;

/*
 * Lay out and (optionally) draw text, line by line.
 *   x, top      where the first line starts
 *   cursor      true for qg_print/println: may scroll, and is remembered
 */
static void run_text(const ctx_t *c, int32_t x, int32_t top, const char *text,
                     style_t st, bool draw, bool cursor, run_result_t *res)
{
    qg_screen_t *scr = c->scr;
    const char   *p   = text;
    int32_t       pen16 = x * 16;
    int32_t       first_top = top;
    line_t        ln;

    res->max_w = 0;
    for (;;) {
        layout_line(c, p, st, pen16, &ln);
        int32_t line_h = ln.ascent + ln.descent;

        /* QuickBasic-style scroll: the line doesn't fit above the bottom. */
        /* (No scrolling while a smaller view is set: see qg_view().) */
        if (draw && cursor && scr->scroll && qg_int_view_full(scr) &&
            top + line_h > scr->height && top > 0) {
            int32_t dy = top + line_h - scr->height;
            if (scr->backend->scroll_up != NULL) {
                /* BUF8: just move the pixels up in RAM. */
                scr->backend->scroll_up(scr, (int16_t)dy, scr->bg_color);
                top -= dy;
            } else if (scr->hist_cap > 0) {
                scroll_up(scr, dy);          /* DIRECT: clear and reprint */
                top -= dy;
            } else {
                /* No memory to reprint from: start again at the top instead. */
                qg_int_fill_rect(scr, 0, 0, scr->width, scr->height, scr->bg_color);
                top = 0;
            }
            first_top = top;
        }

        int32_t shift = 0;
        if (c->align != QG_ALIGN_LEFT) {
            int32_t room = c->right - c->left - ln.width;
            shift = (c->align == QG_ALIGN_CENTER) ? room / 2 : room;
        }
        if (draw) {
            draw_line(c, &ln, top, shift);
            /* Only DIRECT screens need to remember lines for scrolling. */
            if (cursor && scr->backend->scroll_up == NULL) history_add(scr, &ln, top, line_h);
        }
        if (ln.width > res->max_w) res->max_w = ln.width;

        if (ln.next == NULL) {
            /* The next character goes where the pen stopped, trailing spaces
             * included ("HP: " leaves the cursor after the space).         */
            res->end_x  = (ln.pen16_end + 8) / 16 + shift;
            res->end_y  = top;
            res->last_h = line_h;
            break;
        }

        /* On to the next line. */
        top  += line_h;
        pen16 = c->left * 16;
        st    = ln.st_end;
        p     = ln.next;
        if (cursor) {
            scr->hist_open = false;
        }
    }
    res->total_h = top + res->last_h - first_top;
}

/* The style every call starts from. */
static style_t base_style(const qg_screen_t *scr, const qg_font_t *font, qg_color_t color)
{
    style_t st = { .font = font, .mscale = 0, .mcolor = -1,
                   .base_color = color, .fg = scr->fg_color };
    return st;
}

/* ========================================================================== */
/*  Public functions                                                          */
/* ========================================================================== */

void qg_screen_set_tab_width(qg_screen_t *scr, int16_t pixels)
{
    if (scr != NULL) scr->tab_width = (pixels < 1) ? 1 : pixels;
}

void qg_screen_set_wrap(qg_screen_t *scr, bool on)
{
    if (scr != NULL) scr->wrap = on;
}

void qg_screen_set_scroll(qg_screen_t *scr, bool on)
{
    if (scr != NULL) scr->scroll = on;
}

void qg_locate(qg_screen_t *scr, int16_t x, int16_t y)
{
    if (scr == NULL) return;
    scr->cursor_x = x;
    scr->cursor_y = y;
    scr->margin_x = x;
    scr->hist_open = false;
}

void qg_locate_pct(qg_screen_t *scr, uint8_t x, uint8_t y)
{
    if (scr == NULL) return;
    qg_locate(scr, qg_pct_x(scr, x), qg_pct_y(scr, y));
}

/* Shared by print and println. */
static void print_at_cursor(qg_screen_t *scr, const char *text, bool newline)
{
    if (scr == NULL || !scr->ready || scr->fonts[0] == NULL || text == NULL) return;

    ctx_t c = { .scr = scr, .base_font = scr->fonts[0], .left = scr->margin_x,
                .right = qg_int_right(scr), .wrap = scr->wrap, .align = QG_ALIGN_LEFT };
    run_result_t r;
    run_text(&c, scr->cursor_x, scr->cursor_y, text,
             base_style(scr, scr->fonts[0], QG_DEFAULT), true, true, &r);

    scr->cursor_x    = (int16_t)r.end_x;
    scr->cursor_y    = (int16_t)r.end_y;
    scr->last_line_h = (int16_t)r.last_h;
    if (newline) {
        scr->cursor_x = scr->margin_x;
        scr->cursor_y = (int16_t)(r.end_y + r.last_h);
        scr->hist_open = false;
    }
}

void qg_print(qg_screen_t *scr, const char *text)
{
    print_at_cursor(scr, text, false);
}

void qg_println(qg_screen_t *scr, const char *text)
{
    print_at_cursor(scr, text, true);
}

void qg_print_at(qg_screen_t *scr, int16_t x, int16_t y, const char *text,
                  qg_color_t color, const qg_font_t *font)
{
    if (scr == NULL || !scr->ready || text == NULL) return;
    if (font == NULL) font = scr->fonts[0];
    if (font == NULL || qg_font_check(font->data) != QG_OK) return;

    ctx_t c = { .scr = scr, .base_font = font, .left = x, .right = qg_int_right(scr),
                .wrap = scr->wrap, .align = QG_ALIGN_LEFT };
    run_result_t r;
    run_text(&c, x, y, text, base_style(scr, font, color), true, false, &r);
}

void qg_print_box(qg_screen_t *scr, int16_t x, int16_t y, int16_t width,
                   const char *text, qg_align_t align)
{
    if (scr == NULL || !scr->ready || text == NULL || scr->fonts[0] == NULL) return;
    if (width <= 0) return;

    ctx_t c = { .scr = scr, .base_font = scr->fonts[0], .left = x,
                .right = x + width, .wrap = true, .align = align };
    run_result_t r;
    run_text(&c, x, y, text, base_style(scr, scr->fonts[0], QG_DEFAULT), true, false, &r);
}

void qg_print_align(qg_screen_t *scr, int16_t y, const char *text, qg_align_t align)
{
    if (scr == NULL) return;
    /* Align across the view (the whole screen unless qg_view() set one). */
    int32_t left = qg_int_left(scr), right = qg_int_right(scr);
    if (scr->wrap) {
        qg_print_box(scr, (int16_t)left, y, (int16_t)(right - left), text, align);
        return;
    }
    if (!scr->ready || text == NULL || scr->fonts[0] == NULL) return;
    ctx_t c = { .scr = scr, .base_font = scr->fonts[0], .left = left,
                .right = right, .wrap = false, .align = align };
    run_result_t r;
    run_text(&c, left, y, text, base_style(scr, scr->fonts[0], QG_DEFAULT), true, false, &r);
}

void qg_text_measure(qg_screen_t *scr, const char *text, const qg_font_t *font,
                      int16_t max_width, int16_t *w, int16_t *h)
{
    if (w) *w = 0;
    if (h) *h = 0;
    if (scr == NULL || text == NULL) return;
    if (font == NULL) font = scr->fonts[0];
    if (font == NULL || qg_font_check(font->data) != QG_OK) return;

    ctx_t c = { .scr = scr, .base_font = font, .left = 0,
                .right = (max_width > 0) ? max_width : INT16_MAX,
                .wrap = (max_width > 0), .align = QG_ALIGN_LEFT };
    run_result_t r;
    run_text(&c, 0, 0, text, base_style(scr, font, QG_DEFAULT), false, false, &r);
    if (w) *w = (int16_t)r.max_w;
    if (h) *h = (int16_t)r.total_h;
}
