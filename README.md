# Setup Rush, handheld

The [Setup Rush](https://setuprush.com) trading game on a TI-84 Plus CE and a LilyGo T-Deck. Real recorded charts,
one minute a round: hold to ride long, slide to go short, let go to cash out.

One C file is the whole game. The calculator builds it as is; the T-Deck runs the same file on a layer that copies
the calculator's graphics, keypad and file calls.

| Folder | |
|---|---|
| `game/` | the game (`main.c`), shared by both devices |
| `ti84/` | TI-84 Plus CE build, emulator harness, USB install ([README](ti84/README.md)) |
| `tdeck/` | T-Deck firmware: the platform layer, drivers, flashing ([README](tdeck/README.md)) |
| `data/` | the packed charts and fonts both devices ship, and the tools that make them |

## Quick start

```sh
cd ti84 && make && make data        # TI-84 Plus CE: bin/SRUSH.8xp + bin/*.8xv (needs CEdev)
cd tdeck && platformio run          # T-Deck: .pio/build/setup-rush/firmware.bin
```

Machine-specific tool paths go in a git-ignored `local.env` (`set -a; . ./local.env; set +a`): `CEMU_HEADLESS`,
`TI84_ROM`, `TILIBS`, `TDECK_BACKUPS`, `SETUP_RUSH_REPO`.

## License

GPL-3.0 (`LICENSE`). Fonts: Geist Mono and Big Shoulders Display, SIL Open Font License 1.1 (`data/fonts/`). Emoji candles:
[Twemoji](https://github.com/jdecked/twemoji), CC-BY 4.0.
