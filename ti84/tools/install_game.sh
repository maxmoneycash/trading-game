#!/bin/bash
# Install just the game and the font (the chart files are already on the calculator), then start it.
cd "$(dirname "$0")/.."   # ti84/
L=tools/ti-link
for i in $(seq 1 360); do
  if ioreg -p IOUSB -l -w0 | grep -q '"TI-84 Plus CE"'; then
    $L reset >/dev/null 2>&1
    if timeout 20 $L info 2>&1 | grep -q "OS Version"; then break; fi
  fi
  sleep 5
done
timeout 20 $L info 2>&1 | grep -q "OS Version" || { echo "TIMEOUT"; exit 1; }
$L rm SRAUTO >/dev/null 2>&1
for f in bin/SRFNT.8xv bin/SRUSH.8xp; do for t in 1 2 3; do $L reset >/dev/null 2>&1; r=$($L send $f 2>&1 | grep -E "sent|failed"); echo "$r"; echo "$r" | grep -q sent && break; sleep 2; done; done
$L reset >/dev/null 2>&1
$L keys 0x40 0x09 0x09 0xFC9C 0xDA 0xAC 0xAB 0xAE 0xAC 0xA1 0x05 >/dev/null 2>&1
echo "LAUNCHED"
