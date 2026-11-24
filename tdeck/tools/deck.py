"""Drive Setup Rush on a plugged-in T-Deck over USB serial (the test hook in srush_raw / srush_present).
  deck.py OUTDIR step...    steps: start (open the game), key:<chars> (\\r Enter, \\b Backspace), wait:<seconds>,
                            shot:<name> (save the framebuffer as OUTDIR/<name>.png and print the frame rate)"""
import sys, time, struct, serial, glob
from PIL import Image
out = sys.argv[1]
s = serial.Serial(); s.port = glob.glob('/dev/cu.usbmodem*')[0]; s.baudrate = 115200; s.dtr = False; s.rts = False; s.timeout = 0.2
s.open(); time.sleep(8)  # opening the port resets the deck: let it boot
log = s.read(100000)
last = None
for step in sys.argv[2:]:
    kind, _, arg = step.partition(':')
    if kind == 'start': s.write(b'SRUSH\n'); time.sleep(1.5)
    elif kind == 'key': s.write(arg.encode().decode('unicode_escape').encode('latin1')); time.sleep(0.15)
    elif kind == 'wait': time.sleep(float(arg))
    elif kind == 'shot':
        s.reset_input_buffer(); s.write(b'\x02'); buf = b''; t = time.time()
        while time.time() - t < 8:
            buf += s.read(65536); i = buf.find(b'SRFB')
            if i >= 0 and len(buf) >= i + 12 + 153600: break
        i = buf.find(b'SRFB')
        if i < 0 or len(buf) < i + 12 + 153600: print(arg, 'NO FRAME', len(buf), buf[:200]); continue
        ms, frames = struct.unpack('<II', buf[i + 4:i + 12]); px = struct.unpack('<76800H', buf[i + 12:i + 12 + 153600])
        im = Image.new('RGB', (320, 240)); im.putdata([((c >> 11) << 3, ((c >> 5) & 63) << 2, (c & 31) << 3) for c in px]); im.save(f'{out}/{arg}.png')
        fps = (frames - last[1]) * 1000 / (ms - last[0]) if last and ms > last[0] else None
        last = (ms, frames); print(arg, 'frame', frames, 'ms', ms, 'fps since last shot', fps and round(fps, 1))
s.close()
