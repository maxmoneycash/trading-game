import zlib, struct, sys
src, dst = sys.argv[1], sys.argv[2]
d = open(src, 'rb').read(); W, H = 320, 240
rows = b''.join(b'\0' + b''.join(d[(y * W + x) * 4:(y * W + x) * 4 + 3][::-1] for x in range(W)) for y in range(H))
c = lambda t, b: struct.pack('>I', len(b)) + t + b + struct.pack('>I', zlib.crc32(t + b))
open(dst, 'wb').write(b'\x89PNG\r\n\x1a\n' + c(b'IHDR', struct.pack('>IIBBBBB', W, H, 8, 2, 0, 0, 0)) + c(b'IDAT', zlib.compress(rows)) + c(b'IEND', b''))
