/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    qg_image.c
 * @brief   BMP parsing, RLE8 decoding, scaling and drawing.
 *
 * LAYER:   Public API (images)
 * DEPENDS: qg_image.h, qg_internal.h, backend (write_rgb565)
 *
 * ---------------------------------------------------------------------------
 *  THE PIPELINE
 * ---------------------------------------------------------------------------
 *
 *   file rows, in stored order
 *        |
 *        v   decode ONE source row into a row of palette indices
 *        |   (RLE8: expand the runs; plain: just point at the row)
 *        v
 *   which screen rows does this source row cover?
 *        |   1:1 -> exactly one.  Enlarged -> several.  Shrunk -> maybe none.
 *        v
 *   pick source pixels for each visible screen column (nearest neighbour),
 *   look each index up in the image's palette -> RGB565
 *        |
 *        v
 *   gather rows into a block and send it in one transfer
 *   (transparent images: send each run of solid pixels instead)
 *
 * Only one source row is ever held in RAM, so an image of any height costs
 * the same small, fixed amount of memory.
 *
 * WHY ROWS ARE HANDLED IN STORED ORDER
 * RLE8 data can only be read from the start, one row after another, and BMP
 * files normally store the BOTTOM row first. Rather than decode the whole
 * image to find its top row, we draw rows in the order they're stored. The
 * screen doesn't care what order rows arrive in.
 */
#include <string.h>
#include "qg_image.h"
#include "qg_internal.h"
#include "qg_palette.h"

/* ========================================================================== */
/*  Reading the file                                                          */
/* ========================================================================== */

/* BMP numbers are little-endian (low byte first) and may sit at odd
 * addresses, so read them a byte at a time.                                  */
static inline uint32_t rd16(const uint8_t *p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8); }
static inline uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/*
 * BMP LAYOUT (the parts we use)
 *
 *   offset  size  field
 *   0       2     "BM"
 *   10      4     where the pixel data starts
 *   14      4     info header size (40, or 108/124 for newer variants)
 *   18      4     width
 *   22      4     height  (negative = rows stored top row first)
 *   28      2     bits per pixel (must be 8)
 *   30      4     compression (0 = none, 1 = RLE8)
 *   46      4     colours used (0 = all 256)
 *   14+hdr        palette: 4 bytes per colour, in the order B, G, R, 0
 */
qg_err_t qg_image_open(qg_image_t *img, const uint8_t *data, uint32_t size,
                         uint8_t flags)
{
    if (img == NULL || data == NULL || size < 54) return QG_ERR_ARG;
    memset(img, 0, sizeof(*img));

    if (data[0] != 'B' || data[1] != 'M') return QG_ERR_ARG;

    uint32_t pix_ofs  = rd32(data + 10);
    uint32_t hdr_size = rd32(data + 14);
    int32_t  w        = (int32_t)rd32(data + 18);
    int32_t  h        = (int32_t)rd32(data + 22);
    uint32_t bpp      = rd16(data + 28);
    uint32_t comp     = rd32(data + 30);
    uint32_t colours  = rd32(data + 46);

    if (hdr_size < 40 || 14 + hdr_size > size || pix_ofs >= size) return QG_ERR_ARG;
    if (bpp != 8)                                  return QG_ERR_UNSUPPORTED;
    if (comp != 0 && comp != 1)                    return QG_ERR_UNSUPPORTED;
    if (w <= 0 || h == 0)                          return QG_ERR_ARG;
    if (w > QG_IMAGE_MAX_WIDTH)                   return QG_ERR_UNSUPPORTED;

    if (colours == 0 || colours > 256) colours = 256;
    uint32_t pal_ofs = 14 + hdr_size;
    if (pal_ofs + colours * 4 > pix_ofs) {
        /* Palette would overlap the pixels: trust the file layout instead. */
        colours = (pix_ofs - pal_ofs) / 4;
        if (colours == 0) return QG_ERR_ARG;
    }

    img->bottom_up     = (h > 0);
    img->width         = (int16_t)w;
    img->height        = (int16_t)(h > 0 ? h : -h);
    img->rle           = (comp == 1);
    img->palette       = data + pal_ofs;
    img->palette_count = (uint16_t)colours;
    img->pixels        = data + pix_ofs;
    img->end           = data + size;
    img->transparent   = (flags & QG_IMAGE_TRANSPARENT) != 0;

    if (img->rle && !img->bottom_up) return QG_ERR_ARG;   /* not allowed by the format */

    if (!img->rle) {
        /* Uncompressed rows are padded to a multiple of 4 bytes. */
        img->row_stride = ((uint32_t)w + 3u) & ~3u;
        if ((uint32_t)(img->end - img->pixels) < img->row_stride * (uint32_t)img->height) {
            return QG_ERR_ARG;                              /* file too short */
        }
    }
    return QG_OK;
}

/* ========================================================================== */
/*  RLE8 decoding, one row at a time                                          */
/* ========================================================================== */

/*
 * RLE8 IN ONE PARAGRAPH
 * The data is a stream of byte pairs. A pair (n, c) with n > 0 means "n
 * pixels of colour c". A pair starting with 0 is a command:
 *     (0, 0)          end of this row
 *     (0, 1)          end of the image
 *     (0, 2) dx dy    skip dx pixels right and dy rows up; skipped pixels
 *                     are left undrawn (we treat them as transparent)
 *     (0, n>=3)       the next n bytes are n individual pixels ("absolute
 *                     mode"), plus one padding byte if n is odd
 */
typedef struct {
    const uint8_t *p, *end;
    int32_t skip_rows;     /* whole rows still to skip after a delta          */
    int32_t start_x;       /* column the next row starts at, after a delta    */
    bool    done;          /* end of image reached                            */
} rle_t;

static void rle_row(rle_t *r, uint8_t *row, int32_t w, uint8_t blank)
{
    memset(row, blank, (size_t)w);
    if (r->skip_rows > 0) { r->skip_rows--; return; }
    if (r->done) return;

    int32_t x = r->start_x;
    r->start_x = 0;

    while (r->end - r->p >= 2) {
        uint8_t n = r->p[0], c = r->p[1];
        r->p += 2;

        if (n > 0) {                                     /* a run            */
            for (uint8_t i = 0; i < n; i++, x++) {
                if (x >= 0 && x < w) row[x] = c;
            }
        } else if (c == 0) {                             /* end of row       */
            return;
        } else if (c == 1) {                             /* end of image     */
            r->done = true;
            return;
        } else if (c == 2) {                             /* delta            */
            if (r->end - r->p < 2) break;
            int32_t dx = r->p[0], dy = r->p[1];
            r->p += 2;
            if (dy == 0) {
                x += dx;
            } else {
                r->skip_rows = dy - 1;       /* rows passed over entirely  */
                r->start_x   = x + dx;       /* where the target row picks up */
                return;
            }
        } else {                                         /* absolute mode    */
            int32_t cnt = c, padded = cnt + (cnt & 1);
            if (r->end - r->p < cnt) break;
            for (int32_t i = 0; i < cnt; i++, x++) {
                if (x >= 0 && x < w) row[x] = r->p[i];
            }
            r->p += (r->end - r->p < padded) ? cnt : padded;
        }
    }
    r->done = true;                                      /* ran out of data  */
}

/* ========================================================================== */
/*  Drawing                                                                   */
/* ========================================================================== */

static uint8_t  s_src[QG_IMAGE_MAX_WIDTH];       /* one decoded source row   */
static uint8_t  s_idx[QG_IMAGE_MAX_WIDTH];       /* the visible, scaled row  */
static uint16_t s_rgb[QG_IMAGE_MAX_WIDTH];       /* ... as RGB565            */
static uint16_t s_blk[QG_IMAGE_BLOCK_PIXELS];    /* rows gathered for sending*/
static uint16_t s_pal[256];                       /* the image palette, RGB565*/
static uint16_t s_col[QG_IMAGE_MAX_WIDTH];       /* screen column -> source column */

/*
 * Gathering rows into blocks.
 * Rows arrive one at a time, moving either down the screen (top-down files)
 * or up it (bottom-up files). Consecutive rows are packed into s_blk so the
 * block is always in top-to-bottom order, then sent as one rectangle. For
 * rows moving UP the screen, they're filled in from the end of the buffer
 * backwards.
 */
typedef struct {
    qg_screen_t *scr;
    int32_t x, w;          /* visible columns                                  */
    int32_t cap;           /* rows that fit in the block                       */
    int32_t rows;          /* rows gathered so far                             */
    int32_t top;           /* screen row of the block's top row                */
    int32_t last_y;        /* the last row added                               */
    bool    upward;        /* rows arrive bottom to top                        */
} block_t;

static void block_flush(block_t *b)
{
    if (b->rows == 0) return;
    const uint16_t *start = b->upward ? &s_blk[(b->cap - b->rows) * b->w] : s_blk;
    b->scr->backend->write_rgb565(b->scr, (int16_t)b->x, (int16_t)b->top,
                                  (int16_t)b->w, (int16_t)b->rows, start);
    b->rows = 0;
}

static void block_add(block_t *b, int32_t y, const uint16_t *row)
{
    int32_t step = b->upward ? -1 : 1;
    if (b->rows > 0 && (y != b->last_y + step || b->rows == b->cap)) {
        block_flush(b);
    }
    int32_t slot = b->upward ? (b->cap - 1 - b->rows) : b->rows;
    memcpy(&s_blk[slot * b->w], row, (size_t)b->w * sizeof(uint16_t));
    if (b->rows == 0 || b->upward) b->top = y;
    b->last_y = y;
    b->rows++;
}

/*
 * Send one row's solid parts, for transparent images: each run of
 * non-transparent pixels goes as its own rectangle, repeated for `n` screen
 * rows starting at y (n > 1 when the image is enlarged vertically).
 */
static void send_runs(qg_screen_t *scr, int32_t x0, int32_t w, int32_t y, int32_t n)
{
    int32_t i = 0;
    while (i < w) {
        while (i < w && s_idx[i] == 255) i++;            /* skip see-through  */
        int32_t start = i;
        while (i < w && s_idx[i] != 255) i++;            /* a solid run       */
        int32_t len = i - start;
        if (len == 0) continue;

        /* Stack the run n times in the block buffer (in chunks if needed). */
        int32_t per = QG_IMAGE_BLOCK_PIXELS / len;
        if (per < 1) per = 1;
        for (int32_t done = 0; done < n; ) {
            int32_t k = n - done;
            if (k > per) k = per;
            for (int32_t r = 0; r < k; r++) {
                memcpy(&s_blk[r * len], &s_rgb[start], (size_t)len * sizeof(uint16_t));
            }
            scr->backend->write_rgb565(scr, (int16_t)(x0 + start), (int16_t)(y + done),
                                       (int16_t)len, (int16_t)k, s_blk);
            done += k;
        }
    }
}

/*
 * COLOUR MATCHING FOR FRAMEBUFFER SCREENS
 * A BUF8 screen stores palette indices for ITS OWN palette, but an image's
 * pixels are indices into the IMAGE's palette. So each image colour is
 * matched to the nearest screen colour once, giving a 256-entry "remap"
 * table; drawing then just looks every pixel up in it.
 *
 * Building a table means 256 nearest-colour searches of 255 entries each,
 * a few milliseconds, too slow to repeat every frame of an animation. So the
 * last few tables are kept, each labelled with which image palette, which
 * screen, and which version of that screen's palette it was built for. Any
 * palette change bumps the version, so a stale table is never used.
 *
 * The tables themselves belong to the framebuffer backend (qg_internal.h),
 * so programs without framebuffer screens don't carry them.
 */
static const uint8_t *remap_for(const qg_screen_t *scr, const qg_image_t *img)
{
    qg_remap_cache_t *cache = (qg_remap_cache_t *)scr->backend->image_cache;
    for (int i = 0; i < QG_REMAP_CACHE; i++) {
        qg_remap_t *r = &cache->tables[i];
        if (r->scr == scr && r->img_palette == img->palette && r->gen == scr->palette_gen) {
            return r->map;
        }
    }
    qg_remap_t *r = &cache->tables[cache->next];
    cache->next = (uint8_t)((cache->next + 1) % QG_REMAP_CACHE);
    for (int i = 0; i < 256; i++) {
        if (i < img->palette_count) {
            const uint8_t *e = &img->palette[i * 4];          /* B, G, R, 0 */
            /* Round the colour to RGB565 FIRST, exactly as the screen stores
             * its palette, and unpack it the same way the matcher unpacks
             * palette entries. Then a colour copied into the palette (see
             * qg_palette_load_image) compares as a perfect match; without
             * this, rounding could make a different entry look closer.     */
            uint16_t c  = QG_RGB565(e[2], e[1], e[0]);
            uint32_t r5 = (c >> 11) & 0x1F, g6 = (c >> 5) & 0x3F, b5 = c & 0x1F;
            r->map[i] = (uint8_t)qg_color_from_rgb((uint8_t)((r5 << 3) | (r5 >> 2)),
                                                    (uint8_t)((g6 << 2) | (g6 >> 4)),
                                                    (uint8_t)((b5 << 3) | (b5 >> 2)),
                                                    scr->palette);
        } else {
            r->map[i] = 0;
        }
    }
    if (img->transparent) r->map[255] = 255;   /* keep "see-through" as 255 */
    r->scr = scr;
    r->img_palette = img->palette;
    r->gen = scr->palette_gen;
    return r->map;
}

/*
 * The core: draw the image stretched to dw x dh with its top-left at (dx, dy).
 */
static void draw_image(qg_screen_t *scr, const qg_image_t *img,
                       int32_t dx, int32_t dy, int32_t dw, int32_t dh)
{
    if (scr == NULL || !scr->ready || img == NULL || img->pixels == NULL) return;
    if (dw <= 0 || dh <= 0) return;
    const bool buffered = (scr->backend->put_idx_row != NULL);
    if (!buffered && scr->backend->write_rgb565 == NULL) return;

    const int32_t sw = img->width, sh = img->height;

    /* To screen pixels (the view may have moved the origin), then the
     * visible part of the destination, after clipping to the view.         */
    dx += scr->origin_x;
    dy += scr->origin_y;
    int32_t vx0 = dx < scr->view_x0 ? scr->view_x0 : dx;
    int32_t vx1 = dx + dw > scr->view_x1 ? scr->view_x1 : dx + dw;     /* exclusive */
    int32_t vy0 = dy < scr->view_y0 ? scr->view_y0 : dy;
    int32_t vy1 = dy + dh > scr->view_y1 ? scr->view_y1 : dy + dh;
    if (vx1 <= vx0 || vy1 <= vy0) return;
    int32_t vw = vx1 - vx0;
    if (vw > QG_IMAGE_MAX_WIDTH) vw = QG_IMAGE_MAX_WIDTH;

    /* BUF8 screens: image colours -> screen palette indices (cached). */
    const uint8_t *remap = buffered ? remap_for(scr, img) : NULL;

    /* DIRECT screens: the image's palette as RGB565. Missing entries show
     * as black.                                                            */
    for (int i = 0; i < 256 && !buffered; i++) {
        if (i < img->palette_count) {
            const uint8_t *e = &img->palette[i * 4];      /* B, G, R, 0 */
            s_pal[i] = qg_int_out_rgb(scr, e[2], e[1], e[0]);   /* adjusted, if set */
        } else {
            s_pal[i] = 0;
        }
    }

    /*
     * WHICH SOURCE COLUMN DOES EACH SCREEN COLUMN SHOW?
     * Nearest neighbour: screen column c shows source column c * sw / dw.
     * Dividing for every pixel is slow, so the answers go into a table once
     * per draw, found by STEPPING rather than dividing: each screen column
     * moves sw/dw of a source column to the right, so we add sw to a running
     * remainder and move on one source column each time it reaches dw.
     * (This is the same trick Bresenham's line algorithm uses.)
     */
    {
        int32_t num = (vx0 - dx) * sw;           /* first visible column * sw */
        int32_t q = num / dw, r = num % dw;      /* the only division         */
        for (int32_t c = 0; c < vw; c++) {
            s_col[c] = (uint16_t)q;
            r += sw;
            while (r >= dw) { r -= dw; q++; }
        }
    }
    const uint8_t blank = img->transparent ? 255 : 0;

    rle_t   rle = { .p = img->pixels, .end = img->end };
    block_t blk = { .scr = scr, .x = vx0, .w = vw,
                    .cap = QG_IMAGE_BLOCK_PIXELS / vw, .upward = img->bottom_up };
    if (blk.cap < 1) blk.cap = 1;

    for (int32_t s = 0; s < sh; s++) {
        /* Stored row s is image row sy. */
        int32_t sy = img->bottom_up ? (sh - 1 - s) : s;

        /* Screen rows showing image row sy: those y with y * sh / dh == sy,
         * i.e. from ceil(sy * dh / sh) to ceil((sy + 1) * dh / sh) - 1.     */
        /* (32-bit is plenty: at most 480 x 480. The RP2350 divides 32-bit
         * numbers in hardware; 64-bit division is a slow software routine.) */
        int32_t y0 = dy + (sy * dh + sh - 1) / sh;
        int32_t y1 = dy + ((sy + 1) * dh + sh - 1) / sh - 1;
        if (y0 < vy0) y0 = vy0;
        if (y1 > vy1 - 1) y1 = vy1 - 1;

        /* Get the row's pixels. RLE must be read even for rows we won't
         * draw, because each row's data only starts where the last ended.   */
        const uint8_t *src;
        if (img->rle) {
            rle_row(&rle, s_src, sw, blank);
            src = s_src;
        } else {
            if (y0 > y1) continue;               /* plain rows can be skipped */
            src = img->pixels + (uint32_t)s * img->row_stride;
        }
        if (y0 > y1) continue;

        if (buffered) {
            /* Framebuffer: scale, remap to the screen's palette, store the
             * row once for each screen row it covers. See-through pixels
             * stay 255 and are skipped.                                    */
            for (int32_t c = 0; c < vw; c++) {
                s_idx[c] = remap[src[s_col[c]]];
            }
            for (int32_t y = y0; y <= y1; y++) {
                scr->backend->put_idx_row(scr, (int16_t)vx0, (int16_t)y, (int16_t)vw,
                                          s_idx, img->transparent);
            }
            continue;
        }

        /* Scale the visible part of the row and convert it to RGB565:
         * two table lookups per pixel, no arithmetic.                      */
        for (int32_t c = 0; c < vw; c++) {
            uint8_t v = src[s_col[c]];
            s_idx[c] = v;
            s_rgb[c] = s_pal[v];
        }

        if (img->transparent) {
            send_runs(scr, vx0, vw, y0, y1 - y0 + 1);
        } else if (img->bottom_up) {
            for (int32_t y = y1; y >= y0; y--) block_add(&blk, y, s_rgb);
        } else {
            for (int32_t y = y0; y <= y1; y++) block_add(&blk, y, s_rgb);
        }
    }
    block_flush(&blk);
}

/* ========================================================================== */
/*  Public functions                                                          */
/* ========================================================================== */

void qg_image_draw(qg_screen_t *scr, const qg_image_t *img, int16_t x, int16_t y)
{
    if (img == NULL) return;
    draw_image(scr, img, x, y, img->width, img->height);
}

void qg_image_draw_scaled(qg_screen_t *scr, const qg_image_t *img,
                           int16_t x, int16_t y, int16_t w, int16_t h)
{
    draw_image(scr, img, x, y, w, h);
}

void qg_image_draw_fit(qg_screen_t *scr, const qg_image_t *img,
                        int16_t x, int16_t y, int16_t w, int16_t h, qg_align_t align)
{
    if (img == NULL || w <= 0 || h <= 0 || img->width <= 0 || img->height <= 0) return;

    /* Compare shapes without division: is the image wider than the box,
     * relative to their heights? (iw/ih > w/h  <=>  iw*h > w*ih)            */
    int32_t fw, fh;
    if ((int32_t)img->width * h > (int32_t)w * img->height) {
        fw = w;                                        /* width decides */
        fh = (int32_t)img->height * w / img->width;
    } else {
        fh = h;                                        /* height decides */
        fw = (int32_t)img->width * h / img->height;
    }
    if (fw < 1) fw = 1;
    if (fh < 1) fh = 1;

    int32_t ox = (align == QG_ALIGN_CENTER) ? (w - fw) / 2 :
                 (align == QG_ALIGN_RIGHT)  ? (w - fw) : 0;
    int32_t oy = (h - fh) / 2;
    draw_image(scr, img, x + ox, y + oy, fw, fh);
}

int qg_palette_load_image(qg_screen_t *scr, const qg_image_t *img, uint8_t first)
{
    if (scr == NULL || img == NULL || img->palette == NULL) return 0;
    int copied = 0;
    for (int i = 0; i < img->palette_count && first + copied <= 254; i++) {
        if (img->transparent && i == 255) break;          /* never copy "see-through" */
        const uint8_t *e = &img->palette[i * 4];          /* B, G, R, 0 */
        qg_palette_set(scr, (qg_color_t)(first + copied), e[2], e[1], e[0]);
        copied++;
    }
    return copied;
}
