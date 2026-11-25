#!/bin/bash
# Wait for the calculator, then install the chart files and the game and start it.
cd "$(dirname "$0")/.."   # ti84/
L=tools/ti-link
for i in $(seq 1 120); do
  $L reset >/dev/null 2>&1
  if timeout 20 $L info 2>&1 | grep -q "OS Version"; then break; fi
  sleep 5
done
timeout 20 $L info 2>&1 | grep -q "OS Version" || { echo "TIMEOUT: calculator never answered"; exit 1; }
echo "LINK UP"
$L rm SRAUTO SRSHA SRSHP >/dev/null 2>&1
$L rm SRD0 SRD1 SRD2 SRD3 SRD4 SRD5 SRD6 SRD7 SRD8 SRD9 SRDA SRDB SRDC SRDD SRDE SRDF >/dev/null 2>&1
$L send bin/SRD0.8xv bin/SRD1.8xv bin/SRD2.8xv bin/SRD3.8xv bin/SRD4.8xv bin/SRD5.8xv bin/SRD6.8xv bin/SRD7.8xv bin/SRD8.8xv bin/SRD9.8xv bin/SRDA.8xv bin/SRDB.8xv bin/SRDC.8xv bin/SRDD.8xv bin/SRDE.8xv bin/SRDF.8xv bin/SRUSH.8xp 2>&1 | grep -E "sent|failed"
$L ls 2>&1 | grep -E "SRD|SRUSH" | awk -F'|' '{print $3, $4, $6}'
$L keys 0x40 0x09 0x09 0xFC9C 0xDA 0xAC 0xAB 0xAE 0xAC 0xA1 0x05 >/dev/null 2>&1
echo "LAUNCHED"
