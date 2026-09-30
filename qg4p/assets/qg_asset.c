/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    qg_asset.c
 * @brief   Reading the asset pack built by tools/mkpack.py.
 *
 * LAYER:   Assets
 * DEPENDS: qg_asset.h, pico-sdk (XIP_BASE, __flash_binary_end)
 *
 * The pack format is described at the top of tools/mkpack.py. In short: a
 * 32-byte header, then a table of 48-byte entries sorted by name, then the
 * files' data. Everything is read in place from flash.
 */
#include <string.h>
#include "qg_asset.h"

#ifdef QG_ASSET_HOST_TEST
#define XIP_BASE 0x10000000u
extern char __flash_binary_end;
#else
#include "hardware/regs/addressmap.h"   /* XIP_BASE: where flash appears in memory */
/* Defined by the pico-sdk linker script: the first byte after the firmware. */
extern char __flash_binary_end;
#endif

#define HEADER_SIZE  32u
#define ENTRY_SIZE   48u
#define PACK_VERSION 1u

static const uint8_t *s_pack   = NULL;   /* NULL until init succeeds */
static uint16_t       s_count  = 0;
static uint32_t       s_size   = 0;

/* Numbers in the pack are little-endian; read them a byte at a time so the
 * code doesn't depend on alignment.                                        */
static inline uint32_t rd16(const uint8_t *p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8); }
static inline uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static inline const uint8_t *entry(uint16_t i) { return s_pack + HEADER_SIZE + (uint32_t)i * ENTRY_SIZE; }

qg_asset_err_t qg_asset_init_at(const uint8_t *pack)
{
    s_pack = NULL;
    s_count = 0;
    s_size = 0;

    /* Erased flash reads as 0xFF, so a missing pack simply fails the magic. */
    if (pack == NULL || memcmp(pack, "QGPK", 4) != 0) return QG_ASSET_ERR_NO_PACK;
    if (rd16(pack + 4) != PACK_VERSION)               return QG_ASSET_ERR_VERSION;

    uint32_t count = rd16(pack + 6);
    uint32_t total = rd32(pack + 8);
    if (total < HEADER_SIZE + count * ENTRY_SIZE || total > 0x1000000u) {
        return QG_ASSET_ERR_CORRUPT;
    }

    /* Check every entry points inside the pack and has a terminated name.
     * This is quick (a few hundred bytes of table) and means qg_asset_find()
     * can never hand back a pointer outside the pack.                      */
    for (uint32_t i = 0; i < count; i++) {
        const uint8_t *e = pack + HEADER_SIZE + i * ENTRY_SIZE;
        uint32_t ofs = rd32(e + 36), size = rd32(e + 40);
        if (memchr(e, '\0', QG_ASSET_NAME_MAX + 1) == NULL) return QG_ASSET_ERR_CORRUPT;
        if (ofs < HEADER_SIZE || ofs > total || size > total - ofs) return QG_ASSET_ERR_CORRUPT;
    }

    s_pack  = pack;
    s_count = (uint16_t)count;
    s_size  = total;
    return QG_ASSET_OK;
}

qg_asset_err_t qg_asset_init(void)
{
    const uint8_t *pack = (const uint8_t *)(uintptr_t)(XIP_BASE + QG_ASSET_PACK_OFFSET);

    /* SAFETY CHECK: if the firmware has grown past the pack's start, loading
     * the firmware overwrote the start of the pack (or will). Say so plainly
     * rather than reading garbage.                                           */
    if ((uintptr_t)&__flash_binary_end > (uintptr_t)pack) {
        s_pack = NULL;
        return QG_ASSET_ERR_OVERLAP;
    }
    return qg_asset_init_at(pack);
}

static void fill(uint16_t i, qg_asset_t *out)
{
    const uint8_t *e = entry(i);
    out->name  = (const char *)e;
    out->data  = s_pack + rd32(e + 36);
    out->size  = rd32(e + 40);
    out->type  = (uint16_t)rd16(e + 44);
    out->flags = (uint16_t)rd16(e + 46);
}

/*
 * The table is sorted by name, so we can use binary search: compare with the
 * middle entry, then keep only the half that can contain the name. 100 files
 * take at most 7 comparisons.
 */
qg_asset_err_t qg_asset_find(const char *name, qg_asset_t *out)
{
    if (s_pack == NULL) return QG_ASSET_ERR_NOT_READY;
    if (name == NULL)   return QG_ASSET_ERR_NOT_FOUND;

    uint32_t lo = 0, hi = s_count;
    while (lo < hi) {
        uint32_t mid = (lo + hi) / 2;
        int cmp = strcmp(name, (const char *)entry((uint16_t)mid));
        if (cmp == 0) {
            if (out) fill((uint16_t)mid, out);
            return QG_ASSET_OK;
        }
        if (cmp < 0) hi = mid;
        else         lo = mid + 1;
    }
    return QG_ASSET_ERR_NOT_FOUND;
}

uint16_t qg_asset_count(void)     { return s_pack ? s_count : 0; }
uint32_t qg_asset_pack_size(void) { return s_pack ? s_size : 0; }

qg_asset_err_t qg_asset_get(uint16_t i, qg_asset_t *out)
{
    if (s_pack == NULL) return QG_ASSET_ERR_NOT_READY;
    if (i >= s_count)   return QG_ASSET_ERR_NOT_FOUND;
    if (out) fill(i, out);
    return QG_ASSET_OK;
}

/*
 * CRC-32: a checksum that changes if any bit of the data changes. mkpack.py
 * stores the CRC of everything after the header; recomputing it here and
 * comparing tells us whether the pack in flash is exactly what was built.
 *
 * This is the standard CRC-32 (the one zip files use). The 256-entry table
 * speeds it up from 8 steps per byte to 1; it's built on first use.
 */
qg_asset_err_t qg_asset_verify(void)
{
    static uint32_t table[256];
    static bool     table_ready = false;

    if (s_pack == NULL) return QG_ASSET_ERR_NOT_READY;

    if (!table_ready) {
        for (uint32_t n = 0; n < 256; n++) {
            uint32_t c = n;
            for (int k = 0; k < 8; k++) {
                c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            }
            table[n] = c;
        }
        table_ready = true;
    }

    uint32_t crc = 0xFFFFFFFFu;
    for (uint32_t i = HEADER_SIZE; i < s_size; i++) {
        crc = table[(crc ^ s_pack[i]) & 0xFFu] ^ (crc >> 8);
    }
    crc ^= 0xFFFFFFFFu;
    return (crc == rd32(s_pack + 12)) ? QG_ASSET_OK : QG_ASSET_ERR_CORRUPT;
}

const char *qg_asset_err_str(qg_asset_err_t err)
{
    switch (err) {
    case QG_ASSET_OK:            return "OK";
    case QG_ASSET_ERR_NO_PACK:   return "no asset pack in flash";
    case QG_ASSET_ERR_VERSION:   return "asset pack from a newer mkpack.py";
    case QG_ASSET_ERR_CORRUPT:   return "asset pack is damaged";
    case QG_ASSET_ERR_NOT_FOUND: return "not found";
    case QG_ASSET_ERR_OVERLAP:   return "firmware has grown into the asset pack area";
    case QG_ASSET_ERR_NOT_READY: return "asset pack not initialised";
    default:                  return "unknown error";
    }
}
