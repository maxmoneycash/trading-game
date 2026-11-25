#!/bin/bash
set -e
T="${TILIBS:?set TILIBS to a built tilibs checkout}"; B=$T/build; D="$(cd "$(dirname "$0")" && pwd)"
GLIB="$(pkg-config --cflags glib-2.0)"; GLIBL="$(pkg-config --libs glib-2.0)"
clang -arch x86_64 -O2 -I$T/libticables/trunk/src -I$T/libtifiles/trunk/src -I$T/libticalcs/trunk/src -I$T/libticonv/trunk/src -I$B/libticables/trunk -I$B/libtifiles/trunk -I$B/libticalcs/trunk -I$B/libticonv/trunk $GLIB \
  "$D/ti-link.c" -o "$D/ti-link" \
  -L$B/libticables/trunk -L$B/libtifiles/trunk -L$B/libticalcs/trunk -L$B/libticonv/trunk -lticalcs2 -lticables2 -ltifiles2 -lticonv $GLIBL \
  -Wl,-rpath,$B/libticables/trunk -Wl,-rpath,$B/libtifiles/trunk -Wl,-rpath,$B/libticalcs/trunk -Wl,-rpath,$B/libticonv/trunk
