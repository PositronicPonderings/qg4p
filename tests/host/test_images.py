# SPDX-License-Identifier: MIT-0
# SPDX-AI-Disclosure: ai-generated
# SPDX-AI-Model: claude-opus-5-5
# SPDX-AI-Provider: Anthropic
import subprocess, struct, sys, os
import numpy as np
from PIL import Image
SW=SH=480
def rgb565(r,g,b): return ((r&0xF8)<<8)|((g&0xFC)<<3)|(b>>3)
def run(path, flags, dx, dy, dw, dh):
    out = subprocess.run(["./imgtest", path, str(flags), str(dx), str(dy), str(dw), str(dh), "o.raw"], capture_output=True, text=True).stdout.split()
    raw = open("o.raw","rb").read()
    fb = np.frombuffer(raw[:SW*SH*2], dtype=np.uint16).reshape(SH,SW)
    mk = np.frombuffer(raw[SW*SH*2:], dtype=np.uint8).reshape(SH,SW)
    return fb, mk, int(out[0]), int(out[1])
def expected(path, flags, dx, dy, dw, dh):
    im = Image.open(path); im.load()
    idx = np.array(im)                       # palette indices, top-down
    pal = im.getpalette()
    lut = np.array([rgb565(pal[3*i],pal[3*i+1],pal[3*i+2]) if 3*i+2 < len(pal) else 0 for i in range(256)], dtype=np.uint16)
    sh, sw = idx.shape
    exp = np.zeros((SH,SW), np.uint16); drawn = np.zeros((SH,SW), bool)
    for y in range(max(dy,0), min(dy+dh,SH)):
        sy = (y-dy)*sh//dh
        for x in range(max(dx,0), min(dx+dw,SW)):
            sx = (x-dx)*sw//dw
            v = idx[sy,sx]
            if (flags & 1) and v == 255: continue
            exp[y,x] = lut[v]; drawn[y,x] = True
    return exp, drawn
def check(path, flags, dx, dy, dw, dh):
    fb, mk, xf, oob = run(path, flags, dx, dy, dw, dh)
    exp, drawn = expected(path, flags, dx, dy, dw, dh)
    bad_pix = int(np.sum((fb != exp) & drawn))
    wrong_draw = int(np.sum((mk>0) != drawn))
    twice = int(np.sum(mk>1))
    ok = bad_pix==0 and wrong_draw==0 and oob==0 and twice==0
    print(f"{'PASS' if ok else 'FAIL'}  {os.path.basename(path):22s} flags={flags} at ({dx:4d},{dy:4d}) size {dw:3d}x{dh:3d}  transfers={xf:4d}  wrong={bad_pix} coverage={wrong_draw} twice={twice} oob={oob}")
    return ok
if __name__ == "__main__":
    allok = True
    for path, flags, w, h in [(p, int(f), int(w), int(h)) for p, f, w, h in (l.split() for l in sys.stdin if l.strip())]:
        for dx,dy,dw,dh in [(0,0,w,h),(5,7,w*3,h*2),(1,2,max(1,w//2),max(1,h//3)),(3,3,w*2+1,h+7),(-w//3,-h//4,w,h),(SW-w//2,SH-h//2,w,h),(10,10,37,23)]:
            allok &= check(path, flags, dx, dy, dw, dh)
    print("ALL PASS" if allok else "SOME FAILED")
    sys.exit(0 if allok else 1)
