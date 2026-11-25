import struct, sys
def var(name, body):
    data = struct.pack('<H', len(body)) + body
    ent = struct.pack('<HHB', 13, len(data), 0x15) + name.encode().ljust(8, b'\0') + b'\0\0' + struct.pack('<H', len(data)) + data
    return b'**TI83F*\x1a\x0a\x00' + b'self-test'.ljust(42, b'\0') + struct.pack('<H', len(ent)) + ent + struct.pack('<H', sum(ent) & 0xffff)
open(sys.argv[1], 'wb').write(var('SRAUTO', bytes([int(sys.argv[2]), int(sys.argv[3])])))
