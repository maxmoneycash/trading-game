#!/usr/bin/env bash
# Put the Setup Rush build on a plugged-in T-Deck, keeping its settings.
#   tools/setup_rush/flash.sh [--wait MINUTES]
# 1. waits for the deck's serial port (one /dev/cu.usbmodem*),
# 2. reads the whole 16 MB flash to ~/lilyshark-backups/ (the rollback: write it back at 0x0),
# 3. writes .pio/build/t-deck/firmware.bin to the first app slot at 0x10000; if the deck was booting the second
#    slot, clears the OTA choice so it boots the first. NVS (radio profile, chat, the game's save) is untouched.
# Rollback: ESPTOOL write_flash 0x0 ~/lilyshark-backups/<the backup>.bin
set -euo pipefail
repo="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
image="${repo}/.pio/build/t-deck/firmware.bin"
backups="${HOME}/lilyshark-backups"
esptool=("${backups}/.venv/bin/python" -m esptool)
wait_min=0
if [[ "${1:-}" == "--wait" ]]; then wait_min="${2:-30}"; fi

[[ -s "${image}" ]] || { echo "no firmware at ${image}: build with platformio run -e t-deck" >&2; exit 1; }
size=$(stat -f %z "${image}")
(( size < 0x640000 )) || { echo "firmware is ${size} bytes: larger than the 6.25 MB app slot" >&2; exit 1; }

deadline=$(( $(date +%s) + wait_min * 60 ))
while :; do
  shopt -s nullglob; ports=(/dev/cu.usbmodem*); shopt -u nullglob
  if (( ${#ports[@]} == 1 )); then port="${ports[0]}"; break; fi
  if (( ${#ports[@]} > 1 )); then echo "more than one usbmodem port: ${ports[*]}" >&2; exit 1; fi
  (( $(date +%s) < deadline )) || { echo "NO DECK: no /dev/cu.usbmodem* port"; exit 2; }
  sleep 5
done
echo "deck on ${port}"

backup="${backups}/tdeck-flash-$(date +%Y%m%d-%H%M%S).bin"
"${esptool[@]}" --chip esp32s3 --port "${port}" --baud 921600 --before default_reset --after no_reset \
  read_flash 0x0 0x1000000 "${backup}" | tr '\r' '\n' | grep -vE '^[0-9]+ \(' | tail -3
[[ $(stat -f %z "${backup}") == 16777216 ]] || { echo "backup is not 16 MB: not flashing" >&2; exit 1; }
echo "backup: ${backup}"
strings -n 8 "${backup}" | grep -m1 -E '"fw":|Lilyshark starting' || true

# which app slot boots: the OTA data's two 32-byte entries at 0xe000 and 0xf000 (sequence in the first 4 bytes)
slot=$(python3 - "${backup}" <<'PY'
import struct, sys
b = open(sys.argv[1], 'rb').read()
seqs = [struct.unpack('<I', b[o:o + 4])[0] for o in (0xe000, 0xf000)]
live = [s for s in seqs if s != 0xffffffff and s != 0]
print((max(live) - 1) % 2 if live else 0)
PY
)
echo "booting app slot ${slot}"

"${esptool[@]}" --chip esp32s3 --port "${port}" --baud 921600 --before no_reset --after no_reset \
  write_flash 0x10000 "${image}" | tr '\r' '\n' | grep -vE '^Writing at' | tail -4
if [[ "${slot}" != 0 ]]; then
  "${esptool[@]}" --chip esp32s3 --port "${port}" --baud 921600 --before no_reset --after no_reset erase_region 0xe000 0x2000 | tail -1
fi
"${esptool[@]}" --chip esp32s3 --port "${port}" --baud 921600 --before no_reset --after hard_reset read_mac | tail -1
echo "FLASHED ${image}"
