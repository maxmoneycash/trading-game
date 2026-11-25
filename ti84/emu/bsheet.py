import struct, zlib, sys
def bmp(path):
    d = open(path, 'rb').read(); off = struct.unpack('<I', d[10:14])[0]; W, H = 320, 240; row = ((W * 3 + 3) // 4) * 4
    return [b''.join(d[off + (H - 1 - y) * row + x * 3: off + (H - 1 - y) * row + x * 3 + 3][::-1] for x in range(W)) for y in range(H)]
out, names = sys.argv[1], sys.argv[2:]; W, H, G, cols = 320, 240, 6, 3
rows_n = (len(names) + cols - 1) // cols; TW = cols * W + (cols - 1) * G; TH = rows_n * H + (rows_n - 1) * G
img = [bytearray(b'\x30' * TW * 3) for _ in range(TH)]
for i, n in enumerate(names):
    r = bmp(n); ox = (i % cols) * (W + G); oy = (i // cols) * (H + G)
    for y in range(H): img[oy + y][ox * 3: ox * 3 + W * 3] = r[y]
raw = b''.join(b'\0' + bytes(r) for r in img)
c = lambda t, b: struct.pack('>I', len(b)) + t + b + struct.pack('>I', zlib.crc32(t + b))
open(out, 'wb').write(b'\x89PNG\r\n\x1a\n' + c(b'IHDR', struct.pack('>IIBBBBB', TW, TH, 8, 2, 0, 0, 0)) + c(b'IDAT', zlib.compress(raw)) + c(b'IEND', b''))
