#!/usr/bin/env python3
# SPDX-License-Identifier: MIT-0
# SPDX-AI-Disclosure: ai-generated
# SPDX-AI-Model: claude-opus-5-5
# SPDX-AI-Provider: Anthropic
"""
img2bmp8.py - convert an image (PNG, JPG, BMP, GIF...) into an 8-bit BMP
for the QG4P, optionally as a C array to compile in.

REQUIREMENTS
    Python 3 and Pillow:   pip install pillow

EXAMPLES
    # Plain conversion: writes star.bmp (RLE8-compressed)
    python3 tools/img2bmp8.py star.png --out star.bmp

    # Resize to 64 px wide (height follows), and write a C array too
    python3 tools/img2bmp8.py star.png --width 64 --out star.bmp --c-array img_star

    # Pixel art: no dithering, keep hard edges
    python3 tools/img2bmp8.py sprite.png --no-dither --out sprite.bmp

    # Make one colour see-through (for images with no alpha channel)
    python3 tools/img2bmp8.py logo.jpg --key 255,0,255 --out logo.bmp

WHAT IT DOES
    1. Loads the image and (optionally) resizes it.
    2. Finds the transparent pixels: alpha below 50%, or the --key colour.
    3. Reduces the rest to at most 255 colours (256 if nothing is
       transparent), with or without dithering.
    4. Puts transparent pixels at palette index 255, the library's
       see-through index.
    5. Writes an 8-bit BMP, compressed with RLE8 unless --uncompressed, or
       unless compression would make it bigger (busy, dithered images can),
       in which case it's stored plain and the script says so.
    6. With --c-array NAME, also writes NAME.c containing the file's bytes
       as `const uint8_t NAME[]` and `const uint32_t NAME_size`.

    It reports whether the image uses transparency, i.e. whether to open it
    with QG_IMAGE_TRANSPARENT.

DITHERING
    Dithering mixes nearby colours in a fine pattern to fake shades that
    aren't in the palette. It helps photos and smooth gradients, but the
    pattern defeats RLE compression and blurs pixel art. The default is ON
    for images with many colours; use --no-dither for flat art.
"""

import argparse
import os
import struct
import sys

try:
    from PIL import Image
except ImportError:
    sys.exit("This script needs Pillow:  pip install pillow")


def pixels_of(im):
    """All pixels as a flat list. Pillow 12 renamed getdata(); use whichever
    this Pillow has, so the script works on old and new versions."""
    if hasattr(im, "get_flattened_data"):
        return list(im.get_flattened_data())
    return list(im.getdata())


# ---------------------------------------------------------------------------
#  RLE8 encoding
# ---------------------------------------------------------------------------
#  Rows are written bottom row first (BMP's usual order). Each row becomes a
#  series of:
#     (n, c)            n pixels (1..255) of colour c         "encoded mode"
#     (0, n, bytes...)  n (3..255) individual pixels, padded  "absolute mode"
#                       to an even byte count
#  followed by (0, 0) at the end of each row and (0, 1) at the end.

def rle8_encode_row(row):
    out = bytearray()
    i, n = 0, len(row)
    while i < n:
        # Length of the run of identical pixels starting here.
        run = 1
        while i + run < n and run < 255 and row[i + run] == row[i]:
            run += 1
        if run >= 3 or (run == 2 and (i + 2 >= n or row[i + 2] != row[i + 1])):
            out += bytes((run, row[i]))
            i += run
            continue

        # Otherwise gather literal pixels until a run of 3+ starts.
        j = i
        while j < n and j - i < 255:
            if j + 2 < n and row[j] == row[j + 1] == row[j + 2]:
                break
            j += 1
        lit = row[i:j]
        if len(lit) >= 3:
            out += bytes((0, len(lit))) + bytes(lit)
            if len(lit) & 1:
                out += b"\x00"                     # pad to an even length
        else:
            for v in lit:                          # 1 or 2 pixels: runs of 1
                out += bytes((1, v))
        i = j
    out += b"\x00\x00"                             # end of row
    return out


def rle8_encode(rows_top_down):
    data = bytearray()
    for row in reversed(rows_top_down):            # bottom row first
        data += rle8_encode_row(row)
    data += b"\x00\x01"                            # end of image
    return bytes(data)


def plain_rows(rows_top_down, width):
    stride = (width + 3) & ~3
    data = bytearray()
    for row in reversed(rows_top_down):
        data += bytes(row) + b"\x00" * (stride - width)
    return bytes(data)


def write_bmp(path_or_none, width, height, palette_rgb, rows, compress):
    """palette_rgb: list of (r, g, b), up to 256. rows: list of lists, top-down."""
    pixel_data = rle8_encode(rows) if compress else plain_rows(rows, width)
    n_colours = len(palette_rgb)
    pal = bytearray()
    for (r, g, b) in palette_rgb:
        pal += bytes((b, g, r, 0))                 # BMP order: B, G, R, 0

    header_size = 14 + 40
    pix_ofs = header_size + len(pal)
    file_size = pix_ofs + len(pixel_data)

    f = bytearray()
    f += b"BM" + struct.pack("<IHHI", file_size, 0, 0, pix_ofs)
    f += struct.pack("<IiiHHIIiiII",
                     40, width, height,            # positive height: bottom-up
                     1, 8,                         # planes, bits per pixel
                     1 if compress else 0,         # BI_RLE8 or BI_RGB
                     len(pixel_data), 2835, 2835,  # 72 dpi, for image editors
                     n_colours, 0)
    f += pal + pixel_data
    if path_or_none:
        with open(path_or_none, "wb") as fh:
            fh.write(f)
    return bytes(f)


# ---------------------------------------------------------------------------
#  Conversion (also used by mkpack.py)
# ---------------------------------------------------------------------------

def convert(input_path, width=None, height=None, colors=0, dither=None,
            key=None, uncompressed=False):
    """Convert an image file to 8-bit BMP bytes.

    Returns (bmp_bytes, info) where info is a dict with: width, height,
    colours, transparent (bool), dithered (bool), pixel_bytes, palette_bytes.
    `key` is an (r, g, b) tuple or None; `dither` None means "decide".
    """
    img = Image.open(input_path)
    img.load()
    img = img.convert("RGBA")

    # --- 1. resize -------------------------------------------------------------
    if width or height:
        w0, h0 = img.size
        w = width or max(1, round(w0 * height / h0))
        h = height or max(1, round(h0 * width / w0))
        # NEAREST keeps pixel art crisp when enlarging; LANCZOS is smoother
        # for shrinking photos.
        method = Image.NEAREST if (w >= w0 and h >= h0) else Image.LANCZOS
        img = img.resize((w, h), method)
    width, height = img.size

    # --- 2. transparency mask ----------------------------------------------------
    pixels = pixels_of(img)
    clear = [(a < 128) or (key is not None and (r, g, b) == key) for (r, g, b, a) in pixels]
    uses_transparency = any(clear)

    # --- 3. reduce colours ---------------------------------------------------------
    max_colours = colors or (255 if uses_transparency else 256)
    if uses_transparency:
        max_colours = min(max_colours, 255)
    max_colours = max(2, min(max_colours, 256))

    rgb = Image.new("RGB", img.size)
    # See-through pixels get a colour already in the image, so they don't
    # use up a palette entry of their own.
    solid = next(((r, g, b) for (r, g, b, a), c in zip(pixels, clear) if not c), (0, 0, 0))
    rgb.putdata([solid if c else (r, g, b) for (r, g, b, a), c in zip(pixels, clear)])

    distinct = len(set(pixels_of(rgb)))
    if dither is None:
        dither = distinct > max_colours
    q = rgb.quantize(colors=max_colours, method=Image.Quantize.MEDIANCUT,
                     dither=Image.Dither.FLOYDSTEINBERG if dither else Image.Dither.NONE)
    pal_flat = q.getpalette()[: 3 * 256]
    idx = pixels_of(q)
    used = max(idx) + 1
    palette = [tuple(pal_flat[i * 3:i * 3 + 3]) for i in range(used)]

    # --- 4. transparent pixels -> index 255 -----------------------------------------
    if uses_transparency:
        palette += [(0, 0, 0)] * (255 - len(palette))
        palette.append((255, 0, 255))             # 255: magenta, never drawn
        idx = [255 if c else v for v, c in zip(idx, clear)]
    rows = [idx[y * width:(y + 1) * width] for y in range(height)]

    # --- 5. encode -------------------------------------------------------------------
    # RLE8 normally shrinks an image a lot, but on "busy" pixels (dithering,
    # photos) it finds few runs and each encoded pixel costs a little extra,
    # so it can come out BIGGER than plain. Encode both ways and keep the
    # smaller: the library draws either kind.
    plain = write_bmp(None, width, height, palette, rows, False)
    data = plain
    rle_chosen = False
    if not uncompressed:
        rle = write_bmp(None, width, height, palette, rows, True)
        if len(rle) < len(plain):
            data, rle_chosen = rle, True
    info = {"width": width, "height": height, "colours": used,
            "transparent": uses_transparency, "dithered": dither,
            "palette_bytes": 4 * len(palette), "rle": rle_chosen,
            "rle_worse": (not uncompressed) and not rle_chosen,
            "pixel_bytes": len(data) - 54 - 4 * len(palette)}
    return data, info


def c_array_text(name, data, source_name, info):
    """The contents of a .c file holding `data` as a const byte array."""
    lines = ["/* %s: %d x %d, generated by tools/img2bmp8.py from %s.%s */"
             % (name, info["width"], info["height"], source_name,
                " Open with QG_IMAGE_TRANSPARENT." if info["transparent"] else ""),
             "#include <stdint.h>", "",
             "const uint32_t %s_size = %d;" % (name, len(data)),
             "const uint8_t %s[%d] = {" % (name, len(data))]
    for i in range(0, len(data), 16):
        lines.append("    " + ", ".join("0x%02x" % b for b in data[i:i + 16]) + ",")
    lines.append("};")
    return "\n".join(lines) + "\n"


# ---------------------------------------------------------------------------
#  Main
# ---------------------------------------------------------------------------

def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("input", help="image file (PNG, JPG, BMP, GIF, ...)")
    ap.add_argument("--out", required=True, help="output .bmp file")
    ap.add_argument("--width", type=int, help="resize to this width (keeps shape unless --height too)")
    ap.add_argument("--height", type=int, help="resize to this height")
    ap.add_argument("--colors", type=int, default=0,
                    help="maximum colours (default: 255, or 256 with no transparency)")
    ap.add_argument("--dither", dest="dither", action="store_true", default=None,
                    help="force dithering on")
    ap.add_argument("--no-dither", dest="dither", action="store_false",
                    help="turn dithering off (best for pixel art and flat colours)")
    ap.add_argument("--key", help="R,G,B colour to make transparent, e.g. 255,0,255")
    ap.add_argument("--uncompressed", action="store_true", help="write plain (not RLE8) pixels")
    ap.add_argument("--c-array", metavar="NAME", help="also write NAME.c next to the .bmp")
    args = ap.parse_args()

    key = tuple(int(v) for v in args.key.split(",")) if args.key else None
    data, info = convert(args.input, args.width, args.height, args.colors,
                         args.dither, key, args.uncompressed)
    with open(args.out, "wb") as fh:
        fh.write(data)

    if info["width"] > 480:
        print("Warning: images wider than 480 px exceed QG_IMAGE_MAX_WIDTH.")
    raw = info["width"] * info["height"]
    print("%s: %d x %d, %d colours%s, %d bytes = %d pixel data (%s, %.0f%% of %d raw)"
          " + %d palette + 54 header"
          % (os.path.basename(args.out), info["width"], info["height"], info["colours"],
             " + transparent" if info["transparent"] else "", len(data), info["pixel_bytes"],
             "RLE8" if info["rle"] else "plain", 100.0 * info["pixel_bytes"] / raw, raw,
             info["palette_bytes"]))
    if info["rle_worse"]:
        print("  (stored plain: RLE8 would have been bigger for this image)")
    if info["transparent"]:
        print("  -> open with QG_IMAGE_TRANSPARENT")
    if info["dithered"]:
        print("  (dithered; try --no-dither if this is flat art)")

    if args.c_array:
        c_path = os.path.join(os.path.dirname(os.path.abspath(args.out)), args.c_array + ".c")
        with open(c_path, "w") as fh:
            fh.write(c_array_text(args.c_array, data, os.path.basename(args.input), info))
        print("  C array written to %s" % c_path)


if __name__ == "__main__":
    main()
