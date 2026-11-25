import struct, sys
from PIL import Image
blob = open('srfnt.bin', 'rb').read()
nf = blob[3]; offs = struct.unpack_from('<' + 'H' * nf, blob, 4)
BG = (10, 10, 11)
def draw(img, f, s, x, y, col):
    base = offs[f]; lineH, asc, first, count = blob[base:base + 4]
    for ch in s:
        go = struct.unpack_from('<H', blob, base + 4 + 2 * (ord(ch) - first))[0]
        g = base + go; xo, yo, w, h, adv = struct.unpack_from('<bbBBB', blob, g); p = g + 5
        for r in range(h):
            n = blob[p]; p += 1
            for _ in range(n):
                rx, ln = blob[p], blob[p + 1]; p += 2; lv = ln >> 6; ln &= 63
                c = tuple(int(BG[i] + (col[i] - BG[i]) * lv / 3) for i in range(3))
                for q in range(ln):
                    X, Y = x + xo + rx + q, y + yo + r
                    if 0 <= X < img.width and 0 <= Y < img.height: img.putpixel((X, Y), c)
        x += adv
    return x
img = Image.new('RGB', (320, 240), BG)
draw(img, 0, "P&L  LONG 25x  +$684.94  75,950  RSI 47", 6, 6, (138, 138, 143))
draw(img, 0, "DOUBLE TOP  BEARISH 83%  HIT TARGET", 6, 22, (244, 243, 239))
draw(img, 2, "+$1,234", 6, 40, (244, 243, 239))
draw(img, 1, "PATTERN LOCKED", 6, 78, (255, 90, 31))
draw(img, 3, "-$541", 6, 108, (255, 75, 62))
draw(img, 3, "GO", 200, 108, (255, 90, 31))
draw(img, 1, "SETUP RUSH  FREE RUN", 6, 170, (244, 243, 239))
img.resize((640, 480), Image.NEAREST).save('preview.png')
