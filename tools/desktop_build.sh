#!/bin/bash
# Build and run the desktop tests and screen sims (no clock needed). Needs a C++17 compiler and ArduinoJson,
# which `pio pkg install` puts in .pio/libdeps (run any `pio run` once first).
#   bash tools/desktop_build.sh [tl|tf|t5|sim|sim5|gsim|all]      binaries go to .pio/desktop/
#   tl  logic tests          tf  fantasy + batch 4 feed tests     t5  batch 5 feed tests + pet tests
#   sim screen sim (test/sim/out/*.ppm)   sim5  batch 5 screen sim   gsim  games autopilot sim
set -o pipefail
P="$(cd "$(dirname "$0")/.." && pwd)"
O="$P/.pio/desktop"
mkdir -p "$O"
AJ="$P/.pio/libdeps/pixelpressbox/ArduinoJson/src"
F="-std=c++17 -O1 -w -DARDUINOJSON_ENABLE_ARDUINO_STRING=1 -DPIXELPRESSBOX_SIM -I$P/test/native -I$P/test/sim -I$AJ -I$P/src"
NETSTUB="$O/netstub.cpp"
cat > "$NETSTUB" <<'EOF'
#include "Arduino.h"
#include <ArduinoJson.h>
int httpGetJson(const String &, JsonDocument &, const JsonDocument *, const char *) { return -1; }
int httpPostFormJson(const String &, const String &, const String &, JsonDocument &) { return -1; }
EOF
what=${1:-all}
fail=0
build() {  # name, dir, sources...
  local name=$1 dir=$2
  shift 2
  (cd "$dir" && c++ $F -I. "$@" -o "$O/$name" 2>&1 | grep -E "error|Undefined|\"_" | head -30)
  [[ -x "$O/$name" ]] || { echo "BUILD FAILED: $name"; fail=1; return 1; }
}
run() {  # name, dir
  (cd "$2" && "$O/$1") || { echo "FAILED: $1"; fail=1; }
}
S="$P/src"
if [[ $what == tl || $what == all ]]; then
  rm -f "$O/tl"
  build tl "$P/test/native" test_logic.cpp $S/{espn,playlist,scorewatch,desk,weather,holidays,feeds,feeds2,motion,wifinet,astro,fantasy}.cpp && run tl "$P/test/native"
fi
if [[ $what == tf || $what == all ]]; then
  rm -f "$O/tf"
  build tf "$P/test/native" test_fantasy.cpp $S/{fantasy,feeds3,espn,render3,render,desk,holidays,astro}.cpp && run tf "$P/test/native"
fi
if [[ $what == t5 || $what == all ]]; then
  if [[ -f "$P/test/native/test_batch5.cpp" ]]; then
    rm -f "$O/t5"
    build t5 "$P/test/native" test_batch5.cpp $S/{feeds5,espn,desk,holidays}.cpp "$NETSTUB" && run t5 "$P/test/native"
  fi
  if [[ -f "$P/test/native/test_pet.cpp" ]]; then
    rm -f "$O/tp"
    build tp "$P/test/native" test_pet.cpp $S/{pet,render,render3,desk,holidays,espn,astro,feeds3}.cpp "$NETSTUB" && run tp "$P/test/native"
  fi
fi
if [[ $what == sim || $what == all ]]; then
  rm -f "$O/sim"
  mkdir -p "$P/test/sim/out"
  build sim "$P/test/sim" sim.cpp $S/{render,render2,render3,desk,holidays,espn,astro,playlist,fantasy,feeds3}.cpp "$NETSTUB" && run sim "$P/test/sim"
fi
if [[ $what == sim5 || $what == all ]]; then
  # each batch 5 sim (test/sim/sim5_<part>.cpp) builds on its own with its own screens
  mkdir -p "$P/test/sim/out"
  for f in "$P"/test/sim/sim5_*.cpp; do
    [[ -f $f ]] || continue
    part=$(basename "$f" .cpp); part=${part#sim5_}
    srcs=()
    case $part in
      a) srcs=($S/render5a.cpp) ;;
      b) srcs=($S/render5b.cpp) ;;
      d) srcs=($S/render5d.cpp $S/feeds5.cpp) ;;
      pet) srcs=($S/pet.cpp) ;;
    esac
    rm -f "$O/sim5_$part"
    build "sim5_$part" "$P/test/sim" "sim5_$part.cpp" $S/{render,render3,desk,holidays,espn,astro,feeds3}.cpp "${srcs[@]}" "$NETSTUB" && run "sim5_$part" "$P/test/sim"
  done
fi
if [[ $what == gsim || $what == all ]]; then
  rm -f "$O/gsim"
  build gsim "$P/test/sim" games_sim.cpp $S/{render,render3,games,espn,holidays,desk,astro,feeds3}.cpp "$NETSTUB" && run gsim "$P/test/sim"
fi
exit $fail
