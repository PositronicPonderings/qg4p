/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    qg_asset.h
 * @brief   Read-only asset pack in flash: find files by name.
 *
 * LAYER:   Assets (separate from the graphics library; qg4p never calls it)
 * DEPENDS: pico-sdk (for the flash address); nothing from qg4p
 *
 * ---------------------------------------------------------------------------
 *  THE IDEA
 * ---------------------------------------------------------------------------
 *  Images, sounds and game data live in one "pack", built on the PC by
 *  tools/mkpack.py and dragged onto the Pico as a UF2, separately from the
 *  firmware. The pack sits in its own area of flash:
 *
 *      0x10000000  +----------------------+
 *                  |  firmware (<= 1 MB)  |   the .uf2 from your build
 *      0x10100000  +----------------------+  <- QG_ASSET_PACK_OFFSET
 *                  |  asset pack (<= 3 MB)|   assets.uf2 from mkpack.py
 *      0x10400000  +----------------------+
 *
 *  Flash is "memory-mapped": its contents can be read through a pointer like
 *  any array. So qg_asset_find() just hands back a pointer into flash, and a
 *  file is used in place, with no copying and no RAM.
 *
 *  Updating the firmware leaves the pack alone, and vice versa: changing the
 *  artwork means rebuilding and re-dragging only the pack.
 *
 * ---------------------------------------------------------------------------
 *  USE
 * ---------------------------------------------------------------------------
 *      if (qg_asset_init() != QG_ASSET_OK) { ...no pack: tell the user... }
 *
 *      qg_asset_t a;
 *      if (qg_asset_find("icons/star.bmp", &a) == QG_ASSET_OK) {
 *          qg_image_t img;
 *          qg_image_open(&img, a.data, a.size,
 *                         (a.flags & QG_ASSET_FLAG_TRANSPARENT) ? QG_IMAGE_TRANSPARENT : 0);
 *          qg_image_draw(&scr, &img, 10, 10);
 *      }
 */
#ifndef QG_ASSET_H
#define QG_ASSET_H

#include <stdint.h>
#include <stdbool.h>

/**
 * Where the pack starts, as an offset into flash. Must match mkpack.py's
 * --offset (default 0x100000, i.e. 1 MB), and must be past the end of the
 * firmware: qg_asset_init() checks that for you.
 */
#ifndef QG_ASSET_PACK_OFFSET
#define QG_ASSET_PACK_OFFSET 0x100000u
#endif

/** Longest name, in characters (the table stores 36 bytes with the NUL). */
#define QG_ASSET_NAME_MAX 35

/** File types, set by mkpack.py from the file extension. */
enum {
    QG_ASSET_TYPE_OTHER = 0,
    QG_ASSET_TYPE_IMAGE = 1,     /* .bmp (or an image converted to .bmp) */
    QG_ASSET_TYPE_SOUND = 2,     /* .wav, .raw                           */
    QG_ASSET_TYPE_TEXT  = 3,     /* .txt, .json, .csv, .ini              */
};

/** Flags, set by mkpack.py. */
#define QG_ASSET_FLAG_TRANSPARENT 0x0001   /* image: open with QG_IMAGE_TRANSPARENT */

/** One file in the pack. `data` points straight into flash. */
typedef struct {
    const char    *name;     /**< e.g. "icons/star.bmp" (in flash)        */
    const uint8_t *data;     /**< the file's bytes (in flash)             */
    uint32_t       size;     /**< in bytes                                */
    uint16_t       type;     /**< QG_ASSET_TYPE_*                            */
    uint16_t       flags;    /**< QG_ASSET_FLAG_*                            */
} qg_asset_t;

typedef enum {
    QG_ASSET_OK            =  0,
    QG_ASSET_ERR_NO_PACK   = -1,  /**< no pack in flash (never loaded, or erased) */
    QG_ASSET_ERR_VERSION   = -2,  /**< a pack from a newer mkpack.py             */
    QG_ASSET_ERR_CORRUPT   = -3,  /**< damaged pack (bad sizes, or CRC mismatch) */
    QG_ASSET_ERR_NOT_FOUND = -4,  /**< no file with that name                    */
    QG_ASSET_ERR_OVERLAP   = -5,  /**< the firmware has grown into the pack area */
    QG_ASSET_ERR_NOT_READY = -6,  /**< qg_asset_init() hasn't succeeded        */
} qg_asset_err_t;

/**
 * Find and check the pack at QG_ASSET_PACK_OFFSET. Quick: it reads only the
 * header and the table of contents. Call once at start-up.
 */
qg_asset_err_t qg_asset_init(void);

/** Like qg_asset_init(), for a pack at any address (used by tests). */
qg_asset_err_t qg_asset_init_at(const uint8_t *pack);

/** Look up a file by its exact name (case matters). */
qg_asset_err_t qg_asset_find(const char *name, qg_asset_t *out);

/** Number of files in the pack (0 if none). */
uint16_t qg_asset_count(void);

/** File number i (0 .. qg_asset_count()-1), in name order: for listings. */
qg_asset_err_t qg_asset_get(uint16_t i, qg_asset_t *out);

/** Total pack size in bytes (0 if none). */
uint32_t qg_asset_pack_size(void);

/**
 * Check the whole pack against its CRC-32 checksum, to detect a damaged or
 * half-written pack. It reads every byte of the pack from flash, which is
 * the slow part: measured at about 4.7 KB per millisecond on a Pico 2
 * (16.8 KB in 3.6 ms), so roughly 0.2 s per MB, or 0.65 s for a full 3 MB
 * pack. Call it once at start-up if you want it, never before lookups.
 */
qg_asset_err_t qg_asset_verify(void);

/** A short English description of an error, for messages. */
const char *qg_asset_err_str(qg_asset_err_t err);

#endif /* QG_ASSET_H */
