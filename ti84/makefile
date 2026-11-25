# Setup Rush for the TI-84 Plus CE. Needs the CE C toolchain (CEdev) on PATH.
#   make          -> bin/SRUSH.8xp   (keep it in RAM: OS 5 can't run archived programs)
#   make data     -> bin/SRD0..SRDF.8xv + bin/SRFNT.8xv (archived appvars: the charts and the fonts)
NAME = SRUSH
DESCRIPTION = "Setup Rush"
COMPRESSED = YES
ARCHIVED = NO
SRCDIR = ../game
# -fno-inline: LTO inlining added ~29 KB, and the unpacked program must fit in RAM next to its packed copy (README)
CFLAGS = -Wall -Wextra -Oz -fno-inline
include $(shell cedev-config --makefile)

SRD := 0 1 2 3 4 5 6 7 8 9 A B C D E F
data: $(foreach n,$(SRD),bin/SRD$(n).8xv) bin/SRFNT.8xv
bin/SRD%.8xv: ../data/out/srd%.bin
	@mkdir -p bin; convbin -j bin -k 8xv -r -n SRD$* -i $< -o $@
bin/SRFNT.8xv: ../data/fonts/srfnt.bin
	@mkdir -p bin; convbin -j bin -k 8xv -r -n SRFNT -i $< -o $@
.PHONY: data
