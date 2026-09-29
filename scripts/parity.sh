#!/bin/zsh
# OpenGL/Metal parity check. Plays a film in a HIDDEN window (no focus, no
# mouse, no sound) and, every N ticks, renders the world with both
# renderers, saving tickNNNNNN-gl.png, -metal.png, -diff.png and a line in
# parity.csv (tick, width, height, mean abs diff 0-255, % pixels off by >16,
# largest diff). Kills the run if no shot arrives for 40 s.
#   scripts/parity.sh <out-dir> [film] [every-ticks] [speed] [WxH]
set -uo pipefail
ROOT=${0:A:h:h}
OUT=${1:?out dir}; FILM=${2:-$ROOT/data/Scenarios/Marathon 2/Demos/L00.filA}
EVERY=${3:-500}; SPEED=${4:-4}; SIZE=${5:-1280x720}
APP="$ROOT/.deps/DerivedData/Build/Products/Release/Durandal.app/Contents/MacOS/Durandal"
rm -rf "$OUT"; mkdir -p "$OUT"
# Paths go in the environment, never on the command line (see shell_options.cpp)
# Everything goes in the environment: only dash flags on the command line,
# or macOS tries to open the other words as documents (see shell_options.cpp)
SDL_MAC_BACKGROUND_APP=1 DURANDAL_BENCHMARK="$OUT/frames.csv" DURANDAL_BENCHMARK_FILM="$FILM" \
  DURANDAL_BENCHMARK_SHOTS="$OUT" DURANDAL_BENCHMARK_HIDDEN=1 DURANDAL_BENCHMARK_SIZE="$SIZE" \
  DURANDAL_BENCHMARK_SPEED="$SPEED" DURANDAL_BENCHMARK_SHOT_EVERY="$EVERY" \
  "$APP" -s --no-chooser > "$OUT/out.log" 2>&1 &
PID=$!
last=$(date +%s)
while kill -0 $PID 2>/dev/null; do
  sleep 2
  if [[ -f "$OUT/parity.csv" ]]; then
    m=$(stat -f %m "$OUT/parity.csv"); (( m > last )) && last=$m
  fi
  if (( $(date +%s) - last > 40 )); then
    echo "No progress for 40 s - killing run"; kill -9 $PID; break
  fi
done
wait $PID 2>/dev/null; echo "exit $?"
[[ -f "$OUT/parity.csv" ]] && cat "$OUT/parity.csv"
grep -i -E 'assert|error|warning' "$OUT/out.log" | head
