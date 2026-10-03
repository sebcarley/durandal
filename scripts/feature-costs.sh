#!/bin/zsh
# Feature cost runs, HIDDEN (only when the owner has said the screen is not
# free): plays one stretch of a film once per variant in a hidden window,
# rendered off-screen at 1920x1080, uncapped, with GPU stage timing on, and
# keeps each run's summary and stage timings for scripts/feature-costs.py.
# REPEAT=n plays the whole list n times, reversing the order on alternate
# passes, so a drifting machine (thermals, other work) evens out.
#   scripts/feature-costs.sh <out-dir> <film> <end-tick> <label>=<settings> ...
#   e.g. scripts/feature-costs.sh .deps/fc/L06 "$L06" 1200 \
#          rampant=quality_tier=5 no-bounce=quality_tier=5,light_bounce=0
# Settings may start with environment overrides before an '@', e.g.
#   fast=DURANDAL_MATH=fast@quality_tier=5
#   small=DURANDAL_BENCHMARK_SIZE=1280x720@quality_tier=5
# and APP=<binary> there runs another build (a copy kept for A/B runs).
# Runs land in <out-dir>/<label>.<pass>.csv (+ .summary.txt, .gpu.txt).
# Kills a run that has not finished after 300 s.
set -uo pipefail
ROOT=${0:A:h:h}
source "$ROOT/scripts/game.sh"   # GAME=m2|inf|m1
OUT=${1:?out dir}; FILM=${2:?film}; END=${3:?end tick}; shift 3
APP="$(game_app)"
SIZE=${SIZE:-1920x1080}
mkdir -p "$OUT"
variants=("$@")
for pass in {1..${REPEAT:-1}}; do
  order=("${variants[@]}")
  (( pass % 2 == 0 )) && order=("${(Oa)variants[@]}")
  for variant in "${order[@]}"; do
    label=${variant%%=*}; settings=${variant#*=}; extra=()
    if [[ $settings == *@* ]]; then extra=(${(s: :)settings%%@*}); settings=${settings#*@}; fi
    app=$APP
    for e in $extra; do [[ $e == APP=* ]] && app=${e#APP=}; done
    extra=(${extra:#APP=*})
    run="$OUT/$label.$pass"
    print -r -- "$(date +%T) pass $pass $label"
    SDL_MAC_BACKGROUND_APP=1 DURANDAL_BENCHMARK_HIDDEN=1 DURANDAL_BENCHMARK_OFFSCREEN=1 DURANDAL_METAL_DISPLAY=1 \
      DURANDAL_QA=1 DURANDAL_SET="$settings" DURANDAL_GPU_TIMING=1 DURANDAL_BENCHMARK_END_TICK="$END" \
      DURANDAL_BENCHMARK="$run.csv" DURANDAL_BENCHMARK_FILM="$FILM" DURANDAL_BENCHMARK_SIZE="$SIZE" \
      env $extra perl -e 'alarm 300; exec @ARGV' "$app" -s --no-chooser > "$run.log" 2>&1
    [[ -f "$run.csv.summary.txt" ]] || print -r -- "  no summary for $label (see $run.log)"
  done
done
