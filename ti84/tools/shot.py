"""Self-test screenshot: shot.py ID OUT.png  (0 menu, 1 GO, 2 ring, 3 long, 4 cash-out, 5 short, 6 flip, 7 countdown, 8 results)."""
import subprocess, sys, time, struct, os, zlib
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__))); LINK = f'{ROOT}/tools/ti-link'
TMP = sys.argv[3] if len(sys.argv) > 3 else __import__('tempfile').gettempdir()
sid, out = int(sys.argv[1]), sys.argv[2]
def link(*a):
    r = subprocess.run([LINK, *a], capture_output=True, text=True); return r.stdout + r.stderr
def var(name, body):  # minimal 8xv
    data = struct.pack('<H', len(body)) + body
    ent = struct.pack('<HHB', 13, len(data), 0x15) + name.encode().ljust(8, b'\0') + b'\0\0' + struct.pack('<H', len(data)) + data
    return b'**TI83F*\x1a\x0a\x00' + b'self-test'.ljust(42, b'\0') + struct.pack('<H', len(ent)) + ent + struct.pack('<H', sum(ent) & 0xffff)
PULL = os.environ.get('PULL')
p = f'{TMP}/SRAUTO.8xv'; open(p, 'wb').write(var('SRAUTO', bytes([sid])))
if not PULL:
  link('rm', 'SRSHA', 'SRSHP'); link('send', p)
  progs = sorted(l.split('|')[2].strip() for l in link('ls').splitlines() if l.count('|') > 5 and l.split('|')[4].strip() in ('05', '06'))
  downs = ['0x04'] * progs.index('SRUSH')
  link('keys', '0x09', '0x09', '0xfc9c', '0x2D', *downs, '0x05', '0x05')
  time.sleep({0: 5, 1: 6, 2: 14, 3: 17, 4: 20, 5: 30, 6: 44, 7: 63, 8: 72}[sid])
got = {}
for n in ('SRSHA', 'SRSHP'):
    f = f'{TMP}/{n}.8xv'
    if os.path.exists(f): os.remove(f)
    for _ in range(25):
        link('reset')
        if 'got' in link('get', n, f): break
        time.sleep(3)
    b = open(f, 'rb').read(); size = b[72] | (b[73] << 8); got[n] = b[74:74 + size]
px = bytearray()
r = got['SRSHA']
for i in range(0, len(r) - 1, 2): px += bytes([r[i]]) * r[i + 1]
px = bytes(px[:76800]).ljust(76800, b'\0')
halves = [px[:38400], px[38400:], got['SRSHP']]
link('rm', 'SRSHA', 'SRSHP', 'SRAUTO')
pw = halves[2]
pal = {}
for i in range(256):
    c = pw[2 * i] | (pw[2 * i + 1] << 8)
    pal[i] = (((c >> 10) & 31) << 3 | ((c >> 10) & 31) >> 2, ((c >> 5) & 31) << 3 | ((c >> 5) & 31) >> 2, (c & 31) << 3 | (c & 31) >> 2)
px = halves[0] + halves[1]; S = 2
rows = b''.join(b'\0' + b''.join(bytes(pal.get(px[y * 320 + x], (255, 0, 255))) * S for x in range(320)) * 1 for y in range(240) for _ in range(S))
def chunk(t, d): return struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d))
open(out, 'wb').write(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', 320 * S, 240 * S, 8, 2, 0, 0, 0)) + chunk(b'IDAT', zlib.compress(rows)) + chunk(b'IEND', b''))
print('wrote', out)
