# Setup Rush on the TI-84 Plus CE

The Setup Rush trading game (setuprush.com) as a native C program, Terminal theme. Source: `../game/main.c`.

Play: `Asm(prgmSRUSH)` (OS 5.0.1 needs `Asm(`: 2ND 0 for the catalog). LEFT/RIGHT market, ENTER play, CLEAR quit.
Hold 2ND (or UP) past 170 ms = long; hold DOWN = short; DOWN while holding 2ND slides the ride short (let DOWN go to
slide back); let go = cash out; a quick tap pushes the tape.

Same as the web game: 1:00, tape 1.1 bars/s ramping +0.9, taps add tempo, a ride holds 3.4+ bars/s, $25,000 a ride,
3 bps a side, liquidation at -$1,000; hold ring + halo, LONG/SHORT word, marching entry line, P&L wash, ride tag
(LIQ % when close), pulsing LIQ line, NO SETUP, breakeven flash, exit boxes + chips, trails, cash floaters, shake,
hit-stop, slow-mo, COMEBACK, "+$X LEFT", candle close pulse, print edge, live price pill, GO intro, last-5 countdown,
HUD tile (ride while open, total under it, tick meter), PRO clock plate (red under 15 s), hint pill, results card.
Modes (menu UP/DOWN): Free run (today's chart, one shot), Rush (long only), Pro, Sniper (3 bullets, triple size),
Call It (UP/DOWN calls on each lock, 3 lives, points), Blow-Up (bank $10,000, leverage 2x->20x, endless), Career (12
stages with stars and unlocks, Fog/Turbo/Blind twists), Shop (13 themes bought with XP, previewed live), Missions (the
phone's 3 a day, same seed). XP and levels/ranks from progression.js; best per mode; all in SRSAVE ('S5').
Candle Vision: the web engine's own setups per window, computed ahead of time: brackets and names, lock-on reticle + caption + slow-mo, plan preview, TARGET/STOP zones and rungs,
READ + combo, HIT TARGET / STOPPED. Indicators: the Pro rotation (ribbon, bands, supertrend, levels + VWAP) as overlays.
Not ported: sound, haptics, online parts (Explore, Reels, rooms, on-chain), boosts, Daily streak rewards.

## Data
`../data/out/srd0..F.bin`: 36 practice windows + 12 Career windows (2 per market, 18 markets) of recorded charts,
40 + 360 candles each, every candle's 32-step path from its recorded trades (the web engine's barPath, ported to
Python bit for bit), plus Candle Vision setups computed by the app's engine ahead of time. `make data` turns them and
the fonts (Geist Mono, Big Shoulders Display, `../data/fonts`) into archived appvars SRD0..SRDF and SRFNT.
Regenerating the charts (`../data/make_pack.py`) needs a Setup Rush app checkout (`SETUP_RUSH_REPO`).

## Build and send
CE C toolchain ([CEdev](https://ce-programming.github.io/toolchain/)) on PATH, then `make` and `make data` in ti84/.
Keep SRUSH in RAM (OS 5 can't run archived programs); the appvars live in the archive. Needs
[clibs](https://github.com/CE-Programming/libraries/releases) on the calculator.

Memory: a compressed program launches only while its packed size x 2 + the unpack extra fits in free RAM (about
138.7 KB on an empty OS 5.0.1: the variable, the OS's copy at userMem and the decompressor's InsertMem). The build is
132.3 KB; that is why the calculator leaves out what the T-Deck has (`FULL_APP 0`: bots, the market tape, the recap,
the intro) and builds with `-fno-inline`. Check every change in the emulator.

`tools/ti-link` (`TILIBS=... tools/build-ti-link.sh`): `info | ls | send FILES | shot OUT.ppm | keys CODES | get NAME
OUT | rm NAMES | reset | romdump OUT`. After a program exits, the link stays wedged until `reset`.
`tools/install_rest.sh SRD0 ... SRDF SRFNT` sends files one at a time (deletes nothing), then the game, and starts it.

## Test in the emulator first (a crash on the calculator clears its RAM)
- `TI84_ROM`: a ROM dumped from your own calculator (`ti-link romdump`). Never share or commit it.
- `CEMU_HEADLESS`: [CEmu](https://github.com/CE-Programming/CEmu)'s `make -C tests/autotester cemu-headless`.
- `emu/stage.sh`, then `emu/drive2.py PREFIX 6000:shot:menu 6100:keys:enter ...` boots the ROM, sends the build,
  types Asm(prgmSRUSH), presses keys and saves screenshots in emu/run. `emu/drive.py` runs the self-test instead
  (`mkauto.py SRAUTO.8xv 9 MARKET`: a scripted round). `bsheet.py out.png a.bmp ...` makes a contact sheet.
  `make CFLAGS="... -DPROF"` draws per-section ms on screen.
- On hardware: `tools/shot.py ID OUT.png` (ids 0-8 save one run-length frame + palette and quit).

## Traps (each one crashed or hung the game)
- Compiled code uses IY as a frame register; OS routines write flags through IY. Any OS call from game code corrupts
  memory. The compiler emits OS calls for libm AND for float abs patterns (`x < 0 ? -x : x`, a sign-bit mask, even
  through `volatile`, and `a - b < c && b - a < c`): they become the OS's fabs (0x0220F0). No libm, no float abs:
  integers and a sine table. Check: `z80-none-elf-objdump -d -mez80 bin/SRUSH.obj | grep 'call 0x2'` must show
  calls only from __start, exit and kb_Scan.
- graphx text is unclipped: `text()` keeps every string fully on screen.
- Floats are software: everything drawn is integer (Q10 tape, 0..65535 price units, Q16 screen mapping). Triangles
  are slow: chamfers and marks are drawn as rows.
