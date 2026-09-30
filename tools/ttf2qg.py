#!/usr/bin/env python3
# SPDX-License-Identifier: MIT-0
# SPDX-AI-Disclosure: ai-generated
# SPDX-AI-Model: claude-opus-5-5
# SPDX-AI-Provider: Anthropic
"""
ttf2qg.py - convert a TrueType/OpenType font into a 1-bit-per-pixel font
file for the QG4P.

The output .c file uses LVGL's font layout (see qg4p/qg_font_format.h), so it
is interchangeable with fonts made by LVGL's own converter.

REQUIREMENTS
    Python 3 and Pillow:   pip install pillow

EXAMPLES
    # 16-pixel sans font, printable ASCII (the default range)
    python3 tools/ttf2qg.py /usr/share/fonts/truetype/dejavu/DejaVuSans.ttf \\
        --size 16 --name qg_font_sans_16 --out qg4p/fonts/qg_font_sans_16.c

    # add the degree, plus-minus and multiplication signs
    python3 tools/ttf2qg.py DejaVuSans.ttf --size 16 --name my_font \\
        --range 32-126 --chars "°±×" --out my_font.c

THEN
    1. Add the .c file to your build (qg4p/CMakeLists.txt, or your app).
    2. In your code:
           LV_FONT_DECLARE(my_font);
           qg_font_t f = qg_font_create(&my_font, QG_DEFAULT, 1);
           qg_screen_set_font(&scr, 0, &f);

HOW IT WORKS
    Each character is drawn by Pillow in pure black-and-white (no smoothing),
    exactly as it will appear on the screen. The script trims each glyph to
    the smallest box around its pixels, records where that box sits relative
    to the baseline, and packs the pixels 8 to a byte.

SIZE
    --size is the font size in pixels, as Pillow uses it: roughly the height
    from the top of tall letters to the bottom of descenders. The resulting
    line height is printed when the script finishes.

LICENSING
    The glyph shapes come from the font you convert, so its licence applies
    to the output. DejaVu, Liberation and FreeFont fonts may be embedded in
    products, including ones you sell. Check any other font's licence first.
"""

import argparse
import os
import sys

try:
    from PIL import Image, ImageDraw, ImageFont
except ImportError:
    sys.exit("This script needs Pillow:  pip install pillow")


def parse_ranges(range_args, extra_chars):
    """Turn '32-126' style ranges plus extra characters into a sorted list
    of unique code points."""
    points = set()
    for r in range_args:
        for part in r.split(","):
            part = part.strip()
            if not part:
                continue
            if "-" in part:
                lo, hi = part.split("-", 1)
                lo, hi = int(lo, 0), int(hi, 0)
                points.update(range(lo, hi + 1))
            else:
                points.add(int(part, 0))
    for ch in extra_chars:
        points.add(ord(ch))
    return sorted(points)


def render_glyph(font, ch, ascent, size):
    """Draw one character in black and white. Returns
    (advance_px_float, box_w, box_h, ofs_x, ofs_y, rows) where rows is a list
    of lists of 0/1 pixels."""
    pad = size * 2
    img = Image.new("1", (size * 5 + pad, size * 4), 0)
    draw = ImageDraw.Draw(img)
    draw.fontmode = "1"                      # no anti-aliasing: 1 bpp
    origin_x = pad
    baseline_y = size * 2                    # plenty of room above and below
    draw.text((origin_x, baseline_y), ch, font=font, fill=1, anchor="ls")

    advance = font.getlength(ch)
    bbox = img.getbbox()                     # (left, top, right, bottom) or None
    if bbox is None:                         # a space: no pixels at all
        return advance, 0, 0, 0, 0, []

    left, top, right, bottom = bbox
    rows = [[img.getpixel((x, y)) and 1 or 0 for x in range(left, right)]
            for y in range(top, bottom)]
    ofs_x = left - origin_x
    ofs_y = baseline_y - bottom              # box bottom, measured UP from baseline
    return advance, right - left, bottom - top, ofs_x, ofs_y, rows


def pack_bits(rows):
    """Pack pixels MSB-first, 8 per byte, with NO padding between rows."""
    bits = [p for row in rows for p in row]
    out = []
    for i in range(0, len(bits), 8):
        chunk = bits[i:i + 8] + [0] * (8 - len(bits[i:i + 8]))
        byte = 0
        for b in chunk:
            byte = (byte << 1) | b
        out.append(byte)
    return out


def group_cmaps(points):
    """Split code points into cmap entries: runs of consecutive code points
    become 'FORMAT0_TINY' ranges; scattered leftovers share one 'SPARSE_TINY'
    list. Glyph numbers start at 1 (0 is reserved for 'no glyph')."""
    cmaps = []
    runs, run = [], [points[0]]
    for p in points[1:]:
        if p == run[-1] + 1:
            run.append(p)
        else:
            runs.append(run)
            run = [p]
    runs.append(run)

    long_runs = [r for r in runs if len(r) >= 4]
    singles = sorted(p for r in runs if len(r) < 4 for p in r)

    gid = 1
    for r in long_runs:
        cmaps.append({"type": "FORMAT0_TINY", "start": r[0], "length": len(r),
                      "gid": gid, "points": r})
        gid += len(r)
    if singles:
        cmaps.append({"type": "SPARSE_TINY", "start": singles[0],
                      "length": singles[-1] - singles[0] + 1, "gid": gid,
                      "points": singles})
        gid += len(singles)
    return cmaps


def char_label(cp):
    ch = chr(cp)
    if ch == "\\":
        ch = "\\\\"
    if ch == "*":
        return "U+%04X" % cp          # avoid closing the C comment by accident
    return 'U+%04X "%s"' % (cp, ch)


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("ttf", help="TrueType/OpenType font file")
    ap.add_argument("--size", type=int, required=True, help="font size in pixels")
    ap.add_argument("--name", required=True, help="C name of the font, e.g. qg_font_sans_16")
    ap.add_argument("--range", action="append", default=[],
                    help="code point range(s), e.g. 32-126 (default) or 0x20-0x7E,0xB0")
    ap.add_argument("--chars", default="", help="extra characters to include, e.g. '°±×'")
    ap.add_argument("--out", required=True, help="output .c file")
    args = ap.parse_args()

    ranges = args.range or ["32-126"]
    points = parse_ranges(ranges, args.chars)
    if not points:
        sys.exit("No characters selected.")

    font = ImageFont.truetype(args.ttf, args.size)
    ascent, descent = font.getmetrics()
    line_height = ascent + descent

    cmaps = group_cmaps(points)
    ordered = [p for c in cmaps for p in c["points"]]   # glyph order = cmap order

    bitmap, glyphs = [], []
    for cp in ordered:
        adv, bw, bh, ox, oy, rows = render_glyph(font, chr(cp), ascent, args.size)
        if bw > 255 or bh > 255 or not (-128 <= ox <= 127) or not (-128 <= oy <= 127):
            sys.exit("Glyph U+%04X is too large for the format; use a smaller size." % cp)
        glyphs.append({"cp": cp, "index": len(bitmap), "adv": int(round(adv * 16)),
                       "w": bw, "h": bh, "ox": ox, "oy": oy})
        bitmap.extend(pack_bits(rows))

    name = args.name
    guard = name.upper()
    o = []
    o.append("/*" + "*" * 78)
    o.append(" * Size: %d px" % args.size)
    o.append(" * Bpp: 1")
    o.append(" * Source: %s" % os.path.basename(args.ttf))
    o.append(" * Generated by tools/ttf2qg.py (QG4P).")
    o.append(" * Layout: LVGL fmt_txt, uncompressed. The glyph shapes are covered")
    o.append(" * by the source font's licence.")
    o.append(" " + "*" * 78 + "*/")
    o.append("")
    o.append("#ifdef LV_LVGL_H_INCLUDE_SIMPLE")
    o.append('#include "lvgl.h"')
    o.append("#else")
    o.append('#include "lvgl/lvgl.h"')
    o.append("#endif")
    o.append("")
    o.append("#ifndef %s" % guard)
    o.append("#define %s 1" % guard)
    o.append("#endif")
    o.append("")
    o.append("#if %s" % guard)
    o.append("")
    o.append("/*-----------------")
    o.append(" *    BITMAPS")
    o.append(" *----------------*/")
    o.append("")
    o.append("/*Store the image of the glyphs*/")
    o.append("static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {")
    for i, g in enumerate(glyphs):
        nxt = glyphs[i + 1]["index"] if i + 1 < len(glyphs) else len(bitmap)
        data = bitmap[g["index"]:nxt]
        o.append("    /* %s */" % char_label(g["cp"]))
        if data:
            for j in range(0, len(data), 12):
                o.append("    " + ", ".join("0x%02x" % b for b in data[j:j + 12]) + ",")
        o.append("")
    if not bitmap:
        o.append("    0x00")
    o.append("};")
    o.append("")
    o.append("/*---------------------")
    o.append(" *  GLYPH DESCRIPTION")
    o.append(" *--------------------*/")
    o.append("")
    o.append("static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {")
    o.append("    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,")
    for g in glyphs:
        o.append("    {.bitmap_index = %d, .adv_w = %d, .box_w = %d, .box_h = %d, .ofs_x = %d, .ofs_y = %d},"
                 % (g["index"], g["adv"], g["w"], g["h"], g["ox"], g["oy"]))
    o.append("};")
    o.append("")
    o.append("/*---------------------")
    o.append(" *  CHARACTER MAPPING")
    o.append(" *--------------------*/")
    o.append("")
    for k, c in enumerate(cmaps):
        if c["type"] == "SPARSE_TINY":
            offs = [p - c["start"] for p in c["points"]]
            o.append("static const uint16_t unicode_list_%d[] = {" % k)
            o.append("    " + ", ".join("0x%x" % v for v in offs))
            o.append("};")
            o.append("")
    o.append("/*Collect the unicode lists and glyph_id offsets*/")
    o.append("static const lv_font_fmt_txt_cmap_t cmaps[] =")
    o.append("{")
    entries = []
    for k, c in enumerate(cmaps):
        if c["type"] == "SPARSE_TINY":
            entries.append(
                "    {\n        .range_start = %d, .range_length = %d, .glyph_id_start = %d,\n"
                "        .unicode_list = unicode_list_%d, .glyph_id_ofs_list = NULL, .list_length = %d, "
                ".type = LV_FONT_FMT_TXT_CMAP_SPARSE_TINY\n    }"
                % (c["start"], c["length"], c["gid"], k, len(c["points"])))
        else:
            entries.append(
                "    {\n        .range_start = %d, .range_length = %d, .glyph_id_start = %d,\n"
                "        .unicode_list = NULL, .glyph_id_ofs_list = NULL, .list_length = 0, "
                ".type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY\n    }"
                % (c["start"], c["length"], c["gid"]))
    o.append(",\n".join(entries))
    o.append("};")
    o.append("")
    o.append("/*--------------------")
    o.append(" *  ALL CUSTOM DATA")
    o.append(" *--------------------*/")
    o.append("")
    o.append("#if LVGL_VERSION_MAJOR == 8")
    o.append("/*Store all the custom data of the font*/")
    o.append("static  lv_font_fmt_txt_glyph_cache_t cache;")
    o.append("#endif")
    o.append("")
    o.append("#if LVGL_VERSION_MAJOR >= 8")
    o.append("static const lv_font_fmt_txt_dsc_t font_dsc = {")
    o.append("#else")
    o.append("static lv_font_fmt_txt_dsc_t font_dsc = {")
    o.append("#endif")
    o.append("    .glyph_bitmap = glyph_bitmap,")
    o.append("    .glyph_dsc = glyph_dsc,")
    o.append("    .cmaps = cmaps,")
    o.append("    .kern_dsc = NULL,")
    o.append("    .kern_scale = 0,")
    o.append("    .cmap_num = %d," % len(cmaps))
    o.append("    .bpp = 1,")
    o.append("    .kern_classes = 0,")
    o.append("    .bitmap_format = 0,")
    o.append("#if LVGL_VERSION_MAJOR == 8")
    o.append("    .cache = &cache")
    o.append("#endif")
    o.append("};")
    o.append("")
    o.append("/*-----------------")
    o.append(" *  PUBLIC FONT")
    o.append(" *----------------*/")
    o.append("")
    o.append("/*Initialize a public general font descriptor*/")
    o.append("#if LVGL_VERSION_MAJOR >= 8")
    o.append("const lv_font_t %s = {" % name)
    o.append("#else")
    o.append("lv_font_t %s = {" % name)
    o.append("#endif")
    o.append("    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,")
    o.append("    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,")
    o.append("    .line_height = %d,          /*The maximum line height required by the font*/" % line_height)
    o.append("    .base_line = %d,             /*Baseline measured from the bottom of the line*/" % descent)
    o.append("#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)")
    o.append("    .subpx = LV_FONT_SUBPX_NONE,")
    o.append("#endif")
    o.append("#if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8")
    o.append("    .underline_position = -1,")
    o.append("    .underline_thickness = 1,")
    o.append("#endif")
    o.append("    .dsc = &font_dsc,")
    o.append("#if LV_VERSION_CHECK(8, 2, 0) || LVGL_VERSION_MAJOR >= 9")
    o.append("    .fallback = NULL,")
    o.append("#endif")
    o.append("    .user_data = NULL,")
    o.append("};")
    o.append("")
    o.append("#endif /*#if %s*/" % guard)
    o.append("")

    with open(args.out, "w", encoding="utf-8") as f:
        f.write("\n".join(o))

    flash = len(bitmap) + 8 * (len(glyphs) + 1)
    print("%s: %d glyphs, line height %d px, baseline %d px, ~%d bytes of flash"
          % (name, len(glyphs), line_height, descent, flash))


if __name__ == "__main__":
    main()
