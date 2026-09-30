/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    qg_image.h
 * @brief   Drawing images: 8-bit BMP files, plain or RLE8-compressed.
 *
 * LAYER:   Public API (images)
 * DEPENDS: qg_screen.h, qg_text.h (for qg_align_t)
 *
 * ---------------------------------------------------------------------------
 *  THE FORMAT: 8-BIT INDEXED BMP
 * ---------------------------------------------------------------------------
 *  Each image carries its own palette of up to 256 colours, and every pixel
 *  is one byte: an index into that palette. The pixels are usually stored
 *  with RLE8 compression ("run-length encoding": "40 pixels of colour 3"
 *  instead of 40 separate bytes), which shrinks flat-colour art like icons
 *  buttons and sprites a great deal. Uncompressed 8-bit BMPs work too.
 *
 *  Make them with tools/img2bmp8.py (from PNG, JPG, BMP or GIF), or export
 *  from GIMP: Image > Mode > Indexed, then File > Export As .bmp with
 *  "Run-Length Encoded" ticked.
 *
 *  Images are read straight from flash: nothing is copied into RAM.
 *
 * ---------------------------------------------------------------------------
 *  TRANSPARENCY
 * ---------------------------------------------------------------------------
 *  Palette index 255 means "see-through" when the image is opened with
 *  QG_IMAGE_TRANSPARENT; those pixels are skipped and whatever is behind
 *  shows through. Without the flag, index 255 is an ordinary colour, so any
 *  256-colour BMP still draws correctly. img2bmp8.py puts transparent pixels
 *  at 255 and says whether an image needs the flag. In GIMP, the transparent
 *  colour must be the LAST palette entry.
 *
 * ---------------------------------------------------------------------------
 *  USE
 * ---------------------------------------------------------------------------
 *      extern const uint8_t img_icon[];       (from img2bmp8.py --c-array)
 *      extern const uint32_t img_icon_size;
 *
 *      qg_image_t icon;
 *      if (qg_image_open(&icon, img_icon, img_icon_size, QG_IMAGE_TRANSPARENT) == QG_OK) {
 *          qg_image_draw(&scr, &icon, 100, 60);                  // 1:1
 *          qg_image_draw_scaled(&scr, &icon, 10, 10, 128, 64);   // stretched
 *          qg_image_draw_fit(&scr, &icon, 0, 0, 240, 100, QG_ALIGN_CENTER);
 *      }
 *
 *  Scaling uses "nearest neighbour": each screen pixel takes the colour of
 *  the nearest image pixel. Pixel art stays crisp at whole-number scales.
 *
 *  All drawing clips at the screen edges. BMPs are usually stored bottom
 *  row first, so on a DIRECT screen a large image appears from the bottom up.
 *
 *  On a framebuffer (BUF8) screen, images are matched to the screen's own
 *  palette; see qg_palette_load_image() at the end for exact colours.
 */
#ifndef QG_IMAGE_H
#define QG_IMAGE_H

#include "qg_screen.h"
#include "qg_text.h"

/** Flags for qg_image_open(). */
#define QG_IMAGE_TRANSPARENT  0x01   /**< Palette index 255 is see-through. */

/** An opened image. Fill with qg_image_open(); it only points at the data. */
typedef struct {
    const uint8_t *pixels;        /**< Start of the pixel data.                */
    const uint8_t *end;           /**< One past the end of the whole file.     */
    const uint8_t *palette;       /**< Palette entries, 4 bytes each (B,G,R,0).*/
    uint16_t       palette_count; /**< Entries in the palette (1..256).        */
    int16_t        width;
    int16_t        height;
    bool           bottom_up;     /**< Rows stored bottom row first (usual).   */
    bool           rle;           /**< RLE8 compressed.                        */
    bool           transparent;   /**< Skip pixels of index 255.               */
    uint32_t       row_stride;    /**< Uncompressed: bytes per stored row.     */
} qg_image_t;

/**
 * Check a BMP file in memory and fill in an image object.
 *
 * @param data   the file's bytes (e.g. a const array in flash)
 * @param size   the file's size in bytes
 * @param flags  0, or QG_IMAGE_TRANSPARENT
 * @return QG_OK; QG_ERR_ARG if it isn't a readable BMP; QG_ERR_UNSUPPORTED
 *         if it's a BMP this library can't draw (not 8-bit, or too wide).
 */
qg_err_t qg_image_open(qg_image_t *img, const uint8_t *data, uint32_t size,
                         uint8_t flags);

static inline int16_t qg_image_width(const qg_image_t *img)  { return img->width;  }
static inline int16_t qg_image_height(const qg_image_t *img) { return img->height; }

/** Draw at its own size, top-left corner at (x, y). */
void qg_image_draw(qg_screen_t *scr, const qg_image_t *img, int16_t x, int16_t y);

/** Draw stretched to exactly w x h pixels (width and height scale separately). */
void qg_image_draw_scaled(qg_screen_t *scr, const qg_image_t *img,
                           int16_t x, int16_t y, int16_t w, int16_t h);

/**
 * Draw as large as possible inside the box (x, y, w, h) without changing its
 * shape. The leftover space goes to the sides (per `align`: left, centre or
 * right) or is split evenly above and below.
 */
void qg_image_draw_fit(qg_screen_t *scr, const qg_image_t *img,
                        int16_t x, int16_t y, int16_t w, int16_t h, qg_align_t align);

/**
 * FRAMEBUFFER (BUF8) SCREENS: give an image exact colours.
 *
 * A BUF8 screen has ONE palette of 256 colours shared by everything on it,
 * so image colours are matched to the nearest screen colour when drawn.
 * With the standard palette that can be visibly off (up to about 50 of 255
 * per channel). For an important image, such as a full-screen puzzle scene,
 * copy its colours into the screen's palette first:
 *
 *     qg_palette_load_image(&scr, &scene, 16);          // entries 16 onward
 *     qg_image_draw(&scr, &scene, 0, 0);                 // now exact
 *
 * Starting at 16 keeps the 16 named colours (used by text) intact. Entries
 * from `first` onward are overwritten, up to index 254. Returns how many
 * colours were copied. DIRECT screens don't need this: images there always
 * use their own palette.
 */
int qg_palette_load_image(qg_screen_t *scr, const qg_image_t *img, uint8_t first);

#endif /* QG_IMAGE_H */
