import sys, zlib, struct
names = sys.argv[2:]; out = sys.argv[1]; W, H = 320, 240; cols = 3; rows_n = (len(names) + cols - 1) // cols; G = 6
TW, TH = cols * W + (cols - 1) * G, rows_n * H + (rows_n - 1) * G
img = bytearray(b'\x30' * (TW * TH * 3))
for i, n in enumerate(names):
    d = open(n, 'rb').read(); ox = (i % cols) * (W + G); oy = (i // cols) * (H + G)
    for y in range(H):
        row = b''.join(d[(y * W + x) * 4:(y * W + x) * 4 + 3][::-1] for x in range(W))
        p = ((oy + y) * TW + ox) * 3; img[p:p + W * 3] = row
raw = b''.join(b'\0' + bytes(img[y * TW * 3:(y + 1) * TW * 3]) for y in range(TH))
c = lambda t, b: struct.pack('>I', len(b)) + t + b + struct.pack('>I', zlib.crc32(t + b))
open(out, 'wb').write(b'\x89PNG\r\n\x1a\n' + c(b'IHDR', struct.pack('>IIBBBBB', TW, TH, 8, 2, 0, 0, 0)) + c(b'IDAT', zlib.compress(raw)) + c(b'IEND', b''))
