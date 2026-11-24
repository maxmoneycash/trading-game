"""data/setup_rush/srpack.bin: the calculator's chart variables (SRD0..SRDF) and fonts (SRFNT) in one blob the
T-Deck embeds. 'SRPK', u16 n, n x (char name[8], u32 offset, u32 length), then the bodies."""
import struct, sys, pathlib
SRC = pathlib.Path('/Users/maxmohammadi/setup-rush-ti84/data')
items = [(f'SRD{c}', SRC / 'out' / f'srd{c}.bin') for c in '0123456789ABCDEF'] + [('SRFNT', SRC / 'fonts' / 'srfnt.bin')]
items = [(n, p.read_bytes()) for n, p in items if p.exists()]
head = 6 + 16 * len(items); off = head; dirs = b''; body = b''
for n, b in items:
    while (off + len(body)) % 4: body += b'\0'
    dirs += n.encode().ljust(8, b'\0') + struct.pack('<II', off + len(body), len(b)); body += b
blob = b'SRPK' + struct.pack('<H', len(items)) + dirs + body
dst = pathlib.Path(__file__).resolve().parents[2] / 'data/setup_rush/srpack.bin'
dst.parent.mkdir(parents=True, exist_ok=True); dst.write_bytes(blob)
print('wrote', dst, len(blob), 'bytes,', len(items), 'items')
