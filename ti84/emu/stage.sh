#!/bin/sh
# Copy the build (bin/SRUSH.8xp, bin/SRD*.8xv, bin/SRFNT.8xv) into emu/run for drive.py / drive2.py.
set -e; cd "$(dirname "$0")/.."; mkdir -p emu/run; cp bin/SRUSH.8xp bin/*.8xv emu/run/
