#!/usr/bin/env bash
# Put the Setup Rush build on a plugged-in T-Deck, keeping its settings.
#   tools/setup_rush/flash.sh [--env setup-rush|t-deck] [--wait MINUTES] [--backup FILE]
# --env setup-rush (the Setup Rush-only firmware) also erases the second app slot and the OTA choice, so no copy of
# lilyshark is left to boot; NVS stays (the game's save lives there; the backup holds everything else).
# 1. waits for the deck's serial port (one /dev/cu.usbmodem*),
# 2. reads the whole 16 MB flash to ~/lilyshark-backups/ (the rollback: write it back at 0x0),
# 3. writes .pio/build/t-deck/firmware.bin to the first app slot at 0x10000; if the deck was booting the second
#    slot, clears the OTA choice so it boots the first. NVS (radio profile, chat, the game's save) is untouched.
# Rollback: ESPTOOL write_flash 0x0 ~/lilyshark-backups/<the backup>.bin
set -euo pipefail
repo="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
env_name=t-deck
backups="${HOME}/lilyshark-backups"
esptool=("${backups}/.venv/bin/python" -m esptool)
wait_min=0
reuse=""
while [[ $# -gt 0 ]]; do
  case "$1" in
    --wait) wait_min="${2:-30}"; shift 2 ;;
    --backup) reuse="$2"; shift 2 ;;   # a full backup already taken from this deck: skip the 10 minute read
    --env) env_name="$2"; shift 2 ;;
    *) echo "usage: flash.sh [--env NAME] [--wait MINUTES] [--backup FILE]" >&2; exit 2 ;;
  esac
done

image="${repo}/.pio/build/${env_name}/firmware.bin"
[[ -s "${image}" ]] || { echo "no firmware at ${image}: build with platformio run -e ${env_name}" >&2; exit 1; }
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

if [[ -n "${reuse}" ]]; then
  backup="${reuse}"
  "${esptool[@]}" --chip esp32s3 --port "${port}" --baud 921600 --before default_reset --after no_reset read_mac | tail -1
else
  backup="${backups}/tdeck-flash-$(date +%Y%m%d-%H%M%S).bin"
  "${esptool[@]}" --chip esp32s3 --port "${port}" --baud 921600 --before default_reset --after no_reset \
    read_flash 0x0 0x1000000 "${backup}" | tr '\r' '\n' | grep -vE '^[0-9]+ \(' | tail -3
fi
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

# a deck that drops off USB mid-write keeps its bootloader: wait for it to come back and write again (3 tries)
for try in 1 2 3; do
  if "${esptool[@]}" --chip esp32s3 --port "${port}" --baud 921600 --before default_reset --after no_reset \
       write_flash 0x10000 "${image}" > "${backups}/last-write.log" 2>&1; then
    tr '\r' '\n' < "${backups}/last-write.log" | grep -vE '^Writing at' | tail -3; break
  fi
  echo "write ${try} failed: $(tr '\r' '\n' < "${backups}/last-write.log" | grep -v '^Writing at' | tail -1)"
  (( try < 3 )) || { echo "WRITE FAILED: the app slot may be half written; bootloader intact, replug and run again" >&2; exit 1; }
  for _ in $(seq 1 120); do shopt -s nullglob; ports=(/dev/cu.usbmodem*); shopt -u nullglob; (( ${#ports[@]} == 1 )) && break; sleep 5; done
  (( ${#ports[@]} == 1 )) || { echo "WRITE FAILED and the deck did not come back" >&2; exit 1; }
  port="${ports[0]}"; sleep 2
done
if [[ "${env_name}" == setup-rush ]]; then
  "${esptool[@]}" --chip esp32s3 --port "${port}" --baud 921600 --before default_reset --after no_reset erase_region 0x650000 0x640000 | tail -1
  slot=1  # and clear the OTA choice below, so the bootloader takes the first slot
fi
if [[ "${slot}" != 0 ]]; then
  "${esptool[@]}" --chip esp32s3 --port "${port}" --baud 921600 --before default_reset --after no_reset erase_region 0xe000 0x2000 | tail -1
fi
"${esptool[@]}" --chip esp32s3 --port "${port}" --baud 921600 --before default_reset --after hard_reset read_mac | tail -1
echo "FLASHED ${image}"
