"""Like drive.py, but without the self-test: the game starts as a person would see it (intro, menu), and the steps
press keys. Usage: drive2.py OUTPREFIX STEP...   STEP = MS:shot:NAME or MS:keys:k1,k2 (cemu key names)."""
import os, subprocess, sys
HERE = os.path.dirname(os.path.abspath(__file__))
H = os.environ.get('CEMU_HEADLESS') or sys.exit('set CEMU_HEADLESS to a cemu-headless binary (see ti84/README.md)')
ROM = os.environ.get('TI84_ROM') or sys.exit('set TI84_ROM to a ROM dumped from your own calculator')
R = os.environ.get('EMU_RUN', os.path.join(HERE, 'run'))  # emu/stage.sh puts the build here; screenshots land here too
err = open(f'{R}/headless.err', 'w')
p = subprocess.Popen([H, '--rom', ROM], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=err, text=True, bufsize=1)
def cmd(c):
    p.stdin.write(c + '\n'); p.stdin.flush()
    while True:
        l = p.stdout.readline()
        if not l: return 'EOF'
        if l.startswith(('OK', 'ERR')): return l.strip()
while 'READY' not in p.stdout.readline(): pass
print(cmd('run 3000'))
print(cmd(f'send-file {os.environ.get("PLACE", "ram")} {R}/SRUSH.8xp'))
for n in '0123456789ABCDEF': cmd(f'send-file archive {R}/SRD{n}.8xv')
cmd(f'send-file archive {R}/SRFNT.8xv')
print(cmd('run 500'))
print(cmd('keys clear,2nd,0,math,down,down,down,down,down,down,enter,prgm,' + os.environ.get('POS', '1') + ',enter,enter'))
t = 0
for spec in sys.argv[2:]:
    ms, kind, arg = spec.split(':', 2)
    print(cmd(f'run {int(ms) - t}')); t = int(ms)
    if kind == 'shot': print(cmd(f'screenshot {R}/{sys.argv[1]}{arg}.bmp'))
    else: print(cmd(f'keys {arg}'))
print(cmd('run 2000'))
p.stdin.close(); p.wait(timeout=30)
