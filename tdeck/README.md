# Setup Rush on the LilyGo T-Deck

The whole firmware is the game: the deck powers on into the intro, then the menu, then rounds. The radio is held in
reset; nothing transmits.

## Controls

| In a round | |
|---|---|
| Hold the screen | ride long |
| Slide down while holding | flip short (slide back up: long again) |
| Hold the trackball click | ride long |
| Hold W or SPACE | ride long; let go to cash out (keyboards with LilyGo's 2025 firmware; older ones toggle) |
| Hold S | ride short (S while W is down flips short) |
| ENTER | let go (older keyboards) |
| DEL, Q or ESC | end the round |

Menus: tap what you want, swipe or roll the trackball to move, click or ENTER to select, DEL to go back.

## Layout

- `../game/main.c`: the game, the same file the calculator builds. `tools/sync_game.py` turns it into
  `src/platform/srush_game.c` on every build (git-ignored; never edit it).
- `src/platform/srush_shim.c`: the TI-84 CE graphx / keypadc / fileioc calls the game uses, drawn into a 320x240
  RGB565 buffer. `srush_input.c`: touch, trackball and keyboard as the game's keys, plus the thumb ring.
  `srush_audio.c`: the sound synth, 22.05 kHz over I2S.
- `src/app/main.cpp`: display, touch, trackball, keyboard, GPS clock, speaker, saves (NVS namespace `srush`).
- `src/device/`, `include/tdeck/`: T-Deck drivers, from [lilyshark](https://github.com/maxmoneycash/lilyshark).
- `data/srpack.bin`: charts and fonts (`tools/make_pack.py` packs `../data`), linked into flash.
- The daily chart follows the clock, which a GPS fix sets. Without a fix the game uses a fixed day.

## Build, test, flash

```sh
platformio run                                 # in tdeck/
tools/desktop.sh && .pio/srush/srush OUT SCRIPT  # the game on a desktop: scripted input, PPM frames + audio.raw
sim/build-native.sh && .pio/sim/trading-game    # the simulator: play it on your computer (SDL2)
sim/build-web.sh                               # the same in a browser (Emscripten), .pio/web
tools/flash.sh --wait 30                       # full 16 MB backup first, then the app; NVS (the save) is kept
python3 tools/deck.py OUT key:$'\r' wait:5 shot:round   # drive a plugged-in deck over USB
```

`flash.sh` also erases the second app slot and the OTA choice, so the deck can only boot this firmware. Rollback:
`esptool write_flash 0x0 <backup>.bin` with the backup it wrote to `$TDECK_BACKUPS` (default `~/tdeck-backups`).
