# SPDX-License-Identifier: MIT-0
# SPDX-AI-Disclosure: ai-generated
# SPDX-AI-Model: claude-opus-5-5
# SPDX-AI-Provider: Anthropic
"""Build the extra BMP files the image test needs, and list the test cases.
Run from tests/host/build (run_tests.sh does this)."""
import struct, subprocess, sys
from PIL import Image
T = "../../../tools/img2bmp8.py"
IMG = "../../hardware/images"

def bmp(w, h, pal, pixdata, comp):
    palb = b"".join(bytes((b, g, r, 0)) for r, g, b in pal)
    ofs = 54 + len(palb)
    return (b"BM" + struct.pack("<IHHI", ofs + len(pixdata), 0, 0, ofs) +
            struct.pack("<IiiHHIIiiII", 40, w, h, 1, 8, comp, len(pixdata), 0, 0, len(pal), 0) + palb + pixdata)

pal = [(i * 16 % 256, (i * 40) % 256, 255 - i * 12 % 256) for i in range(20)]
# A top-down plain BMP (negative height), 7x5, rows padded to 8 bytes.
rows = [[(x + 2 * y) % 20 for x in range(7)] for y in range(5)]
open("topdown.bmp", "wb").write(bmp(7, -5, pal, b"".join(bytes(r) + b"\0" for r in rows), 0))
# An RLE8 stream using runs, odd and even literal runs, and a delta (skip) code.
s = bytearray()
s += bytes((3, 1, 0, 5, 2, 3, 4, 5, 6, 0, 1, 7)) + b"\0\0"
s += bytes((0, 4, 9, 8, 7, 6)) + bytes((5, 2)) + b"\0\0"
s += bytes((2, 11, 0, 2, 3, 2)) + bytes((4, 12)) + b"\0\0"
s += bytes((9, 14)) + b"\0\0" + b"\0\1"
open("rle_delta.bmp", "wb").write(bmp(9, 6, pal, bytes(s), 1))

cases = []
for name, transparent in (("d20", 1), ("potion", 1), ("banner", 0), ("landscape", 0)):
    src = "%s/%s.bmp" % (IMG, name)
    subprocess.run([sys.executable, T, "%s/%s.png" % (IMG, name), "--no-dither", "--uncompressed",
                    "--out", "plain_%s.bmp" % name], check=True, capture_output=True)
    for path, flags in ((src, transparent), ("plain_%s.bmp" % name, transparent)) + (((src, 0),) if transparent else ()):
        w, h = Image.open(path).size
        cases.append("%s %d %d %d" % (path, flags, w, h))
cases.append("topdown.bmp 0 7 5")
open("cases.txt", "w").write("\n".join(cases) + "\n")
print("%d image cases" % len(cases))
