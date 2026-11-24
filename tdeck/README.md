# Setup Rush on the T-Deck

The Setup Rush trading game, built into the device firmware. Open it from the orange SETUP RUSH button on Home, or press `$`.

## Controls

| In a round | |
|---|---|
| Hold the screen | ride long |
| Slide down while holding | flip short (slide back up: long again) |
| Hold the trackball click | ride long |
| W or SPACE | ride long until pressed again |
| S | ride short, or flip a long short |
| ENTER | let go |
| DEL, Q or ESC | end the round |

Menus: tap what you want, swipe or roll the trackball to move, click or ENTER to select, DEL to go back.

## Where things live

- Game logic: `~/setup-rush-ti84/game/src/main.c`, the same source as the TI-84 CE build. `tools/setup_rush/sync_game.py` generates `src/setup_rush/srush_game.c`. Never edit the generated file.
- `src/setup_rush/srush_shim.c`: the graphx/keypadc/fileioc calls the game uses, drawn into a 320x240 RGB565 buffer.
- `src/setup_rush/srush_input.c`: touch, trackball and keyboard mapped onto the game's keys, plus the thumb ring.
- `src/setup_rush/srush_audio.c`: the web game's sound synth, 22.05 kHz over I2S.
- `data/setup_rush/srpack.bin`: charts and fonts, built by `tools/setup_rush/make_pack.py` and linked into flash.
- Glue (`run_setup_rush`, display push, saves in NVS namespace `srush`): device section of `src/sim_main.cpp`.
- Date: the daily chart follows the system clock, which a GPS fix sets (`src/device/hardware_status.cpp`). Without a fix the game uses a fixed day.

## Build, test, flash

```sh
python3 tools/setup_rush/sync_game.py          # after changing main.c
cc -O2 -w -Isrc/setup_rush tools/setup_rush/desktop.c src/setup_rush/srush_*.c -o .pio/srush/srush -lm
.pio/srush/srush OUTDIR SCRIPT                 # scripted input on the desktop, PPM frames + audio.raw
uvx --from platformio==6.1.19 --with intelhex platformio run -e t-deck
tools/setup_rush/flash.sh --wait 30            # full 16 MB backup first, then the app slot only; settings kept
python tools/setup_rush/deck.py OUTDIR start key:$'\r' wait:5 shot:round   # drive the real deck over USB
```

`src/assets/map_tiles_baked.cpp` is git-ignored. Copy it from the main checkout before a device build, or the firmware ships without offline maps.

Rollback: `esptool write_flash 0x0 <backup>.bin`, using the backup `flash.sh` wrote to `~/lilyshark-backups/`.
