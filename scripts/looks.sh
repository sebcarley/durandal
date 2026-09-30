#!/bin/zsh
# Renders the same film ticks with the Metal display under several settings,
# for side-by-side looks: one HIDDEN run per variant, at a fixed 30 fps,
# with the gate open (DURANDAL_QA=1) and settings from DURANDAL_SET.
# ON_SCREEN=1 plays each run full screen on the main display instead;
# END_TICK=n stops each run after tick n.
#   scripts/looks.sh <out-dir> <film|""> <every> <WxH> <label>=<settings> ...
#   e.g. scripts/looks.sh .deps/looks "" 300 1280x720 \
#          stock="shading_tables=0,texel_lighting=0" banded="shading_tables=1,shading_style=0"
# Frames land in <out-dir>/<label>/tickNNNNNN-frame-metal.png. Kills a run
# with no new frame for 40 s. SPEED=n plays the film n times faster.
set -uo pipefail
ROOT=${0:A:h:h}
OUT=${1:?out dir}; FILM=${2:-$ROOT/data/Scenarios/Marathon 2/Demos/L00.filA}
[[ -z $FILM ]] && FILM="$ROOT/data/Scenarios/Marathon 2/Demos/L00.filA"
EVERY=${3:-300}; SIZE=${4:-1280x720}; shift 4
APP="$ROOT/.deps/DerivedData/Build/Products/Release/Durandal.app/Contents/MacOS/Durandal"
for variant in "$@"; do
  label=${variant%%=*}; settings=${variant#*=}
  dir="$OUT/$label"; rm -rf "$dir"; mkdir -p "$dir"
  # ON_SCREEN=1: full screen on the main display (the owner watches), no
  # hidden window; <WxH> "native" for the display's own size
  hidden=(SDL_MAC_BACKGROUND_APP=1 DURANDAL_BENCHMARK_HIDDEN=1)
  [[ ${ON_SCREEN:-0} == 1 ]] && hidden=()
  env $hidden DURANDAL_QA=1 DURANDAL_SET="$settings" DURANDAL_METAL_DISPLAY=1 \
    DURANDAL_BENCHMARK="$dir/run.csv" DURANDAL_BENCHMARK_FILM="$FILM" DURANDAL_BENCHMARK_END_TICK="${END_TICK:-}" \
    DURANDAL_BENCHMARK_SIZE="$SIZE" DURANDAL_BENCHMARK_FPS=30 DURANDAL_BENCHMARK_SPEED="${SPEED:-1}" DURANDAL_BENCHMARK_SHOT_EVERY="$EVERY" \
    DURANDAL_BENCHMARK_FRAMESHOTS="$dir/" "$APP" -s --no-chooser > "$dir/run.log" 2>&1 &
  PID=$!; last=$(date +%s); count=0
  while kill -0 $PID 2>/dev/null; do
    sleep 2
    n=$(ls "$dir" | grep -c -- "-frame-metal.png")
    (( n != count )) && { count=$n; last=$(date +%s); }
    if (( $(date +%s) - last > 40 )); then echo "$label: no progress for 40 s - killing"; kill -9 $PID; break; fi
  done
  wait $PID 2>/dev/null; echo "$label: exit $? ($count frames)"
done
