#!/bin/zsh
# Whole-frame comparison, OpenGL vs Metal display: plays the same film twice
# in a HIDDEN window at a fixed 30 fps (so each captured tick shows the same
# game state), saving the finished frame (HUD, overlays, fades included)
# every N ticks, then compares the pairs. Kills a run with no new frame
# for 40 s.  scripts/frames.sh <out-dir> [film] [every] [WxH]
set -uo pipefail
ROOT=${0:A:h:h}
OUT=${1:?out dir}; FILM=${2:-$ROOT/data/Scenarios/Marathon 2/Demos/L00.filA}
EVERY=${3:-450}; SIZE=${4:-1280x720}
APP="$ROOT/.deps/DerivedData/Build/Products/Release/Durandal.app/Contents/MacOS/Durandal"
rm -rf "$OUT"; mkdir -p "$OUT"
for mode in gl metal; do
  [[ $mode == metal ]] && display=1 || display=0
  SDL_MAC_BACKGROUND_APP=1 DURANDAL_METAL_DISPLAY=$display DURANDAL_BENCHMARK="$OUT/$mode.csv" \
    DURANDAL_BENCHMARK_FILM="$FILM" DURANDAL_BENCHMARK_HIDDEN=1 DURANDAL_BENCHMARK_SIZE="$SIZE" \
    DURANDAL_BENCHMARK_FPS=30 DURANDAL_BENCHMARK_SHOT_EVERY="$EVERY" DURANDAL_BENCHMARK_FRAMESHOTS="$OUT" DURANDAL_BENCHMARK_SHOW_FPS=${SHOW_FPS:-} \
    "$APP" -s --no-chooser > "$OUT/$mode.log" 2>&1 &
  PID=$!; last=$(date +%s); count=0
  while kill -0 $PID 2>/dev/null; do
    sleep 2
    n=$(ls "$OUT" | grep -c -- "-frame-$mode.png")
    (( n != count )) && { count=$n; last=$(date +%s); }
    if (( $(date +%s) - last > 40 )); then echo "$mode: no progress for 40 s - killing"; kill -9 $PID; break; fi
  done
  wait $PID 2>/dev/null; echo "$mode run exit $? ($count frames)"
done
xcrun swift "$ROOT/scripts/compare-frames.swift" "$OUT"
