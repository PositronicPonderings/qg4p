#!/usr/bin/env python3
# SPDX-License-Identifier: MIT-0
# SPDX-AI-Disclosure: ai-generated
# SPDX-AI-Model: claude-opus-5-5
# SPDX-AI-Provider: Anthropic
"""Draw the M6 test images (PNG) from scratch, so they carry no licence
questions. Run from this folder:  python3 make_test_images.py
Then convert them with tools/img2bmp8.py (see convert_test_images.sh)."""
import math
from PIL import Image, ImageDraw, ImageFont

FONT = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"
def font(size):
    try:
        return ImageFont.truetype(FONT, size)
    except OSError:
        return ImageFont.load_default()

# d20: a shaded icosahedron silhouette on a transparent background
im = Image.new("RGBA", (64, 64), (0, 0, 0, 0))
d = ImageDraw.Draw(im)
c, r = 32, 30
hexpts = [(c + r * math.cos(math.radians(a)), c + r * math.sin(math.radians(a))) for a in range(-90, 270, 60)]
d.polygon(hexpts, fill=(150, 30, 40, 255))
tri = [hexpts[0], hexpts[2], hexpts[4]]
d.polygon(tri, fill=(200, 45, 55, 255))
for i in range(6):
    d.line([hexpts[i], hexpts[(i + 1) % 6]], fill=(255, 220, 220, 255), width=2)
for p in tri:
    for q in tri:
        d.line([p, q], fill=(255, 220, 220, 255), width=1)
d.text((32, 36), "20", font=font(16), fill=(255, 255, 255, 255), anchor="mm")
im.save("d20.png")

# potion: 16x16 pixel art, drawn pixel by pixel
art = [
    "................",
    "......####......",
    "......#ww#......",
    ".......##.......",
    "......#..#......",
    ".....#....#.....",
    "....#......#....",
    "...#rrrrrrrr#...",
    "..#rrRRrrrrrr#..",
    "..#rRRrrrrrrr#..",
    "..#rrrrrrrrrr#..",
    "..#rrrrrrrrrd#..",
    "...#rrrrrrrd#...",
    "....##dddd##....",
    "......####......",
    "................",
]
pal = {"#": (40, 30, 60, 255), "w": (200, 170, 120, 255), "r": (200, 40, 80, 255),
       "R": (255, 150, 180, 255), "d": (130, 20, 50, 255), ".": (0, 0, 0, 0)}
im = Image.new("RGBA", (16, 16))
im.putdata([pal[ch] for row in art for ch in row])
im.save("potion.png")

# banner: gradient with lettering
im = Image.new("RGB", (240, 48))
for x in range(240):
    t = x / 239
    col = (int(90 + 60 * t), int(30 + 20 * t), int(160 - 60 * t))
    ImageDraw.Draw(im).line([(x, 0), (x, 47)], fill=col)
d = ImageDraw.Draw(im)
d.text((120, 26), "DICE ROLLER", font=font(26), fill=(0, 0, 0), anchor="mm")
d.text((118, 24), "DICE ROLLER", font=font(26), fill=(255, 215, 90), anchor="mm")
im.save("banner.png")

# landscape: smooth sky, sun, layered hills (the hard case for 256 colours)
W, H = 240, 160
im = Image.new("RGB", (W, H))
px = im.load()
for y in range(H):
    for x in range(W):
        t = y / H
        px[x, y] = (int(40 + 200 * t), int(90 + 110 * t), int(200 - 40 * t))
d = ImageDraw.Draw(im)
d.ellipse([160, 30, 200, 70], fill=(255, 230, 150))
for i, (base, amp, col) in enumerate([(100, 20, (70, 110, 80)), (120, 15, (50, 90, 60)), (140, 10, (35, 70, 45))]):
    pts = [(x, base - amp * math.sin(x / (25 + 10 * i) + i)) for x in range(0, W + 1, 4)]
    d.polygon(pts + [(W, H), (0, H)], fill=col)
im.save("landscape.png")
print("wrote d20.png potion.png banner.png landscape.png")
