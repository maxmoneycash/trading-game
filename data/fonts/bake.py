"""Bake the app's fonts (setup-rush-app: Geist Mono, Big Shoulders Display) into SRFNT for the TI-84 CE.
Each glyph: 2-bit anti-aliased coverage, stored as runs per row (x, len | level << 6), so the calculator draws a solid
interior with memset and only the edge pixels one at a time.
Format: 'SF1', u8 nfonts, nfonts x u16 offset; font: u8 lineH, u8 ascent, u8 first, u8 count, count x u16 glyph offset
(from the font's start, 0 = none); glyph: i8 xoff, i8 yoff (from the line top), u8 w, u8 h, u8 advance, then per row
u8 nruns and nruns x (u8 x, u8 len|level<<6)."""
import io, os, struct, sys
from fontTools.ttLib import TTFont
from fontTools.varLib import instancer
from PIL import Image, ImageDraw, ImageFont

NM = os.path.join(os.environ.get('SETUP_RUSH_REPO') or sys.exit('set SETUP_RUSH_REPO'), 'setup-rush-app/node_modules/@fontsource-variable')
MONO = f'{NM}/geist-mono/files/geist-mono-latin-wght-normal.woff2'
DISP = f'{NM}/big-shoulders-display/files/big-shoulders-display-latin-wght-normal.woff2'

def ttf(path, wght):
    f = TTFont(path)
    f = instancer.instantiateVariableFont(f, {'wght': wght})
    f.flavor = None
    b = io.BytesIO(); f.save(b); b.seek(0)
    return b

# (file, weight, pixel size, tracking px): sizes match the old graphx scales 1, 2, 3, 4 on screen
SPECS = [(MONO, 700, 11, 0), (DISP, 800, 22, 1), (MONO, 800, 26, 0), (DISP, 900, 46, 1)]
FIRST, COUNT = 32, 95

def bake(path, wght, px, track):
    font = ImageFont.truetype(ttf(path, wght), px)
    asc, desc = font.getmetrics()
    lineH = asc + desc
    out = bytearray(struct.pack('<BBBB', lineH, asc, FIRST, COUNT))
    table_at = len(out); out += bytes(2 * COUNT)
    for i in range(COUNT):
        ch = chr(FIRST + i)
        adv = int(round(font.getlength(ch))) + track
        S = 4  # supersample for the coverage
        big = ImageFont.truetype(ttf(path, wght), px * S)
        l, t, r, b = big.getbbox(ch, anchor='la')
        if r <= l or b <= t:
            g = struct.pack('<bbBBB', 0, 0, 0, 0, adv)
        else:
            W = (r - l + S - 1) // S + 1; H = (b - t + S - 1) // S + 1
            ox, oy = l // S, t // S
            img = Image.new('L', (W * S, H * S), 0)
            ImageDraw.Draw(img).text((-ox * S, -oy * S), ch, font=big, fill=255, anchor='la')
            small = img.resize((W, H), Image.BOX)
            px_ = small.load()
            rows = []
            for y in range(H):
                runs, x = [], 0
                while x < W:
                    v = px_[x, y]; lv = 0 if v < 40 else 1 if v < 110 else 2 if v < 190 else 3
                    if not lv: x += 1; continue
                    x0 = x
                    while x < W and x - x0 < 63:
                        v2 = px_[x, y]; l2 = 0 if v2 < 40 else 1 if v2 < 110 else 2 if v2 < 190 else 3
                        if l2 != lv: break
                        x += 1
                    runs.append((x0, x - x0, lv))
                rows.append(runs)
            while rows and not rows[-1]: rows.pop()
            top = 0
            while top < len(rows) and not rows[top]: top += 1
            rows = rows[top:]
            g = bytearray(struct.pack('<bbBBB', ox, oy + top, W, len(rows), adv))
            for runs in rows:
                g.append(len(runs))
                for x0, n, lv in runs: g += bytes([x0, n | (lv << 6)])
        struct.pack_into('<H', out, table_at + 2 * i, len(out))
        out += g
    return bytes(out), lineH, asc

blob = bytearray(b'SF1' + bytes([len(SPECS)])); offs_at = len(blob); blob += bytes(2 * len(SPECS))
for n, spec in enumerate(SPECS):
    data, lh, asc = bake(*spec)
    struct.pack_into('<H', blob, offs_at + 2 * n, len(blob))
    blob += data
    print(f'font {n}: {spec[2]}px line {lh} ascent {asc}, {len(data)} bytes', file=sys.stderr)
open(sys.argv[1], 'wb').write(blob)
print(len(blob), 'bytes', file=sys.stderr)
