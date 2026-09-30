# SPDX-License-Identifier: MIT-0
# SPDX-AI-Disclosure: ai-generated
# SPDX-AI-Model: claude-opus-5-5
# SPDX-AI-Provider: Anthropic
"""RLE8 delta codes, checked against the BMP specification by hand (Pillow's
own delta handling has a bug, so it can't be the reference here)."""
import sys, numpy as np
from PIL import Image
import test_images as ti
fb, mk, _, _ = ti.run("rle_delta.bmp", 0, 0, 0, 9, 6)
im = Image.open("rle_delta.bmp"); pal = im.getpalette()
lut = {ti.rgb565(pal[3*i], pal[3*i+1], pal[3*i+2]): i for i in range(20)}
got = np.array([[lut.get(int(fb[y, x]), -1) for x in range(9)] for y in range(6)])
exp = np.array([[14]*9, [0]*5 + [12]*4, [0]*9, [11, 11] + [0]*7,
                [9, 8, 7, 6, 2, 2, 2, 2, 2], [1, 1, 1, 2, 3, 4, 5, 6, 7]])
ok = (got == exp).all()
print("RLE8 delta file matches the BMP specification" if ok else "delta MISMATCH\n%s" % got)
sys.exit(0 if ok else 1)
