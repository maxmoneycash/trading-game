#!/bin/sh
# Build the desktop harness (the game + this platform layer, scripted input, PPM frames + audio.raw):
#   tools/desktop.sh && .pio/srush/srush OUTDIR SCRIPT
set -e; cd "$(dirname "$0")/.."
python3 tools/sync_game.py >/dev/null
mkdir -p .pio/srush
cc -O2 -w -Isrc/platform tools/desktop.c src/platform/srush_*.c -o .pio/srush/srush -lm
echo .pio/srush/srush
