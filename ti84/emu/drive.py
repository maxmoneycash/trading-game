"""Drive cemu-headless: boot the dumped ROM, send files, launch SRUSH, run, screenshot. Usage: drive.py OUTPREFIX ms:shot ..."""
import subprocess, sys, os
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
print(cmd(f'send-file ram {R}/'+os.environ.get('PRG','SRUSH')+'.8xp'), cmd(f'send-file ram {R}/SRAUTO.8xv'))
if os.environ.get('DATA'):
    for n in '0123456789ABCDEF': cmd(f'send-file archive {R}/SRD{n}.8xv')
    cmd(f'send-file archive {R}/SRFNT.8xv')
print(cmd('run 500'))
print(cmd('keys clear,2nd,0,math,down,down,down,down,down,down,enter,prgm,'+os.environ.get('POS','2')+',enter,enter'))
t = 0
for spec in sys.argv[2:]:
    ms, name = spec.split(':')
    print(cmd(f'run {int(ms) - t}')); t = int(ms)
    print(cmd(f'screenshot {R}/{sys.argv[1]}{name}.bmp'))
cmd('quit')
