#!/bin/bash
# Finish an install that stopped partway: send each listed file (one per link session, 3 tries), then the game, then start it.
# Deletes nothing. Usage: tools/install_rest.sh SRD7 SRD8 ... (names in data/8xv); the game is always sent last.
cd "$(dirname "$0")/.."   # ti84/
L=tools/ti-link
end=$(( $(date +%s) + 6*3600 ))
until ioreg -p IOUSB -l 2>/dev/null | grep -q '"TI-84 Plus CE"'; do (( $(date +%s) < end )) || { echo "calculator not on USB in 6 h"; exit 1; }; sleep 10; done
sleep 3
files=(); for n in "$@"; do files+=("bin/$n.8xv"); done; files+=(bin/SRUSH.8xp)
for f in "${files[@]}"; do
  ok=0
  for t in 1 2 3 4 5; do
    $L reset >/dev/null 2>&1
    out=$(timeout 120 $L send "$f" 2>&1 | grep -E "sent|failed")
    echo "$f try $t: $out"
    if echo "$out" | grep -q sent; then ok=1; break; fi
    sleep 5
  done
  (( ok )) || { echo "STOPPED at $f"; exit 1; }
done
$L reset >/dev/null 2>&1
$L keys 0x40 0x09 0x09 0xFC9C 0xDA 0xAC 0xAB 0xAE 0xAC 0xA1 0x05 >/dev/null 2>&1
echo "INSTALLED AND LAUNCHED"
