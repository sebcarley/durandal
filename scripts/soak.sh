#!/bin/zsh
# Soak test: plays every film under tests/replays (Tooncinator films, saved
# games, net games) hidden, off-screen and uncapped, with the given Durandal
# settings (default: the Flagship tier) and sound running through SDL's
# silent dummy driver, so the audio code (reverb included) is exercised
# without playing anything. Records each run's exit, peak memory and any
# error lines, and lists new crash reports.
#   scripts/soak.sh <out-dir> [DURANDAL_SET settings] [speed]
# Summary: <out-dir>/soak.md. Nothing appears on screen.
set -uo pipefail
ROOT=${0:A:h:h}
OUT=${1:?out dir}; SETTINGS=${2:-quality_tier=3}; SPEED=${3:-4}
APP="$ROOT/.deps/DerivedData/Build/Products/Release/Durandal.app/Contents/MacOS/Durandal"
mkdir -p "$OUT"
MD="$OUT/soak.md"
start=$(date +%s)
{
  echo "# Soak: $SETTINGS, speed $SPEED"; echo
  echo "- Date: $(date '+%Y-%m-%d %H:%M')"
  echo "- Commit: $(git -C "$ROOT" rev-parse --short HEAD) ($(git -C "$ROOT" rev-parse --abbrev-ref HEAD))"
  echo
  echo "| Film | Exit | Seconds | Peak MB | Frames | Errors |"
  echo "|---|---|---|---|---|---|"
} > "$MD"
touch "$OUT/.start"
find "$ROOT/tests/replays/Marathon 2" -name "*.filA" | sort | while read -r film; do
  name=$(basename "$film" .filA)
  safe=${name//[^A-Za-z0-9._-]/_}
  t0=$(date +%s)
  SDL_AUDIODRIVER=dummy SDL_MAC_BACKGROUND_APP=1 DURANDAL_QA=1 DURANDAL_SET="$SETTINGS" DURANDAL_METAL_DISPLAY=1 \
    DURANDAL_BENCHMARK="$OUT/$safe.csv" DURANDAL_BENCHMARK_FILM="$film" DURANDAL_BENCHMARK_HIDDEN=1 \
    DURANDAL_BENCHMARK_SIZE=1920x1080 DURANDAL_BENCHMARK_SPEED="$SPEED" DURANDAL_BENCHMARK_OFFSCREEN=1 \
    /usr/bin/time -l perl -e 'alarm 1200; exec @ARGV' "$APP" --no-chooser > "$OUT/$safe.log" 2> "$OUT/$safe.time"
  code=$?
  secs=$(( $(date +%s) - t0 ))
  peak=$(awk '/maximum resident set size/ {printf "%d", $1/1048576}' "$OUT/$safe.time")
  frames=$(grep -m1 "frames logged" "$OUT/$safe.csv.summary.txt" 2>/dev/null | sed 's/.*logged: \([0-9]*\).*/\1/')
  errors=$(grep -i -c -E "error|failed|assert" "$OUT/$safe.log")
  echo "| $name | $code | $secs | ${peak:-?} | ${frames:-none} | $errors |" >> "$MD"
  echo "$name: exit $code, ${secs}s, peak ${peak:-?} MB, errors $errors"
done
{
  echo
  echo "- Total: $(( ($(date +%s) - start) / 60 )) min"
  echo "- New crash reports: $(find ~/Library/Logs/DiagnosticReports -name 'Durandal*' -newer "$OUT/.start" 2>/dev/null | wc -l | tr -d ' ')"
} >> "$MD"
echo "Done -> $MD"
