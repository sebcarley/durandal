#!/bin/zsh
# Feature cost table, ON SCREEN (the owner watches): plays one stretch of a film
# full screen on the main display once per variant, uncapped, with the gate
# open, GPU stage timing on, and prints average fps, 1% low and the GPU
# world pass for each. Ask the owner before running it.
#   scripts/costs.sh <out-dir> <film> <end-tick> <label>=<settings> ...
#   e.g. scripts/costs.sh .deps/costs "$L06" 1200 base=quality_tier=3 \
#          no-relief=quality_tier=3,surface_relief=0
set -uo pipefail
ROOT=${0:A:h:h}
source "$ROOT/scripts/game.sh"   # GAME=m2|inf|m1
OUT=${1:?out dir}; FILM=${2:?film}; END=${3:?end tick}; shift 3
APP="$(game_app)"
mkdir -p "$OUT"
printf "%-16s %8s %8s %8s %10s %10s  %s\n" variant avg-fps 1%-low worst-s gpu-frame gpu-world thermal-before | tee "$OUT/costs.txt"
for variant in "$@"; do
  label=${variant%%=*}; settings=${variant#*=}
  therm=$(xcrun swift "$ROOT/scripts/thermal.swift" 2>/dev/null | grep -v warning | sed 's/thermal state: //')
  DURANDAL_QA=1 DURANDAL_SET="$settings" DURANDAL_GPU_TIMING=1 DURANDAL_BENCHMARK_END_TICK="$END" \
    DURANDAL_BENCHMARK="$OUT/$label.csv" DURANDAL_BENCHMARK_FILM="$FILM" DURANDAL_BENCHMARK_SIZE=native \
    "$APP" -s --no-chooser > "$OUT/$label.log" 2>&1
  s="$OUT/$label.csv.summary.txt"; g="$OUT/$label.csv.gpu.txt"
  avg=$(grep -m1 'average fps' "$s" 2>/dev/null | sed 's/.*: //')
  low=$(grep -m1 '1% low' "$s" 2>/dev/null | sed 's/.*: \([0-9.]*\) fps.*/\1/')
  worst=$(grep -m1 'worst second' "$s" 2>/dev/null | sed 's/.*: \([0-9]*\) frames.*/\1/')
  frame=$(grep -m1 'whole frame' "$g" 2>/dev/null | sed 's/.*average *\([0-9.]*\) ms.*/\1/')
  world=$(grep -m1 'world pass' "$g" 2>/dev/null | sed 's/.*average *\([0-9.]*\) ms.*/\1/')
  printf "%-16s %8s %8s %8s %10s %10s  %s\n" "$label" "${avg:-?}" "${low:-?}" "${worst:-?}" "${frame:-?}" "${world:-?}" "${therm:-?}" | tee -a "$OUT/costs.txt"
done
