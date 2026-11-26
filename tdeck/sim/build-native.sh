#!/bin/sh
# The simulator as a desktop app (SDL2): sim/build-native.sh && .pio/sim/trading-game
set -e; cd "$(dirname "$0")/.."
python3 tools/sync_game.py >/dev/null; mkdir -p .pio/sim
ARCH=""; case "$(file "$(sdl2-config --prefix)/lib/libSDL2.dylib" 2>/dev/null)" in *x86_64*) ARCH="-arch x86_64";; esac
cc $ARCH -O2 -w -Isrc/platform $(sdl2-config --cflags) sim/sim.c src/platform/srush_*.c -o .pio/sim/trading-game $(sdl2-config --libs) -lm
echo .pio/sim/trading-game
