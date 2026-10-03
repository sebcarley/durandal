#!/bin/zsh
# Hidden, uncapped benchmark of one film (development comparison only; the
# hidden window's numbers are indicative, not a substitute for the real
# on-screen benchmark). Kills the run after 400 s.
#   scripts/bench-hidden.sh <out-prefix> <gl|bridge|metal> [film] [WxH]
# gl = OpenGL; bridge = Metal world inside the OpenGL frame (2a);
# metal = Metal display mode, no OpenGL at all (2b), rendered off-screen and
# never presented (a hidden window's drawables are throttled by macOS).
set -uo pipefail
ROOT=${0:A:h:h}
source "$ROOT/scripts/game.sh"   # GAME=m2|inf|m1
OUT=${1:?out}; MODE=${2:?gl|bridge|metal}; FILM=${3:-$GAME_DEMO}; SIZE=${4:-1920x1080}
APP="$(game_app)"
mkdir -p "${OUT:h}"
SDL_MAC_BACKGROUND_APP=1 DURANDAL_BENCHMARK="$OUT.csv" DURANDAL_BENCHMARK_FILM="$FILM" DURANDAL_BENCHMARK_HIDDEN=1 \
  DURANDAL_BENCHMARK_SIZE="$SIZE" DURANDAL_BENCHMARK_RENDERER=$([[ $MODE == gl ]] && echo gl || echo metal) \
  DURANDAL_METAL_DISPLAY=$([[ $MODE == metal ]] && echo 1 || echo 0) DURANDAL_BENCHMARK_OFFSCREEN=1 \
  perl -e 'alarm 400; exec @ARGV' "$APP" -s --no-chooser > "$OUT.log" 2>&1
cat "$OUT.csv.summary.txt" 2>/dev/null || { echo "no summary"; tail -5 "$OUT.log"; }
