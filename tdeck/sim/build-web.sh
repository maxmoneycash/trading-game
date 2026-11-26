#!/bin/sh
# The browser simulator: sim/sim.c + the platform layer + the game, compiled to WebAssembly with Emscripten.
#   sim/build-web.sh [OUTDIR]   (default .pio/web; needs emcc on PATH, e.g. source emsdk_env.sh)
set -e; cd "$(dirname "$0")/.."
OUT="${1:-.pio/web}"; mkdir -p "$OUT"
python3 tools/sync_game.py >/dev/null
emcc -O2 -w -Isrc/platform sim/sim.c src/platform/srush_*.c -o "$OUT/index.html" \
  -sUSE_SDL=2 -sASYNCIFY -sALLOW_MEMORY_GROWTH -sINVOKE_RUN=0 -sEXIT_RUNTIME=0 -sENVIRONMENT=web \
  -sEXPORTED_RUNTIME_METHODS=callMain -sSTACK_SIZE=4MB -sASYNCIFY_STACK_SIZE=262144 $EMFLAGS \
  --embed-file data/srpack.bin@/srpack.bin --shell-file sim/shell.html
ls -la "$OUT"
