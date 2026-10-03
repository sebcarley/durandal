#!/bin/zsh
# Standard Durandal benchmark: plays the standard films with --benchmark at
# 1920x1080 (windowed, exact pixels) and native full screen, uncapped, no
# sound. OPENS A GAME WINDOW - ask the owner before running it. Stock settings
# unless DURANDAL_QA=1 DURANDAL_SET=... are set (e.g. quality_tier=3).
#   scripts/benchmark.sh <label> [sizes...]     e.g. baseline-0 1920x1080 native
# Per-frame CSVs go to .deps/bench/<label>/, summaries to
# docs/benchmarks/<label>.md. The owner's own films (tests/benchmark-films) are in
# the set. Takes ~20 min per size, plus a ~3.5 min
# untimed warm-up film first so a fanless Mac is thermally settled
# (skip with WARMUP=0).
set -euo pipefail
ROOT=${0:A:h:h}
source "$ROOT/scripts/game.sh"   # GAME=m2|inf|m1
LABEL=${1:?label}; shift
SIZES=(${@:-1920x1080 native})
APP="$(game_app)"
M2="$ROOT/data/Scenarios/Marathon 2"
FILMS=(
  "$M2/Demos/L00.filA"
  "$ROOT/tests/replays/Marathon 2/Tooncinator Films/M2 L06 We're Everywhere.46946.filA"
  "$ROOT/tests/replays/Marathon 2/Net Games/2023-08-06 5D space.56028.filA"
  "$ROOT/tests/benchmark-films/260926-1.filA"
  "$ROOT/tests/benchmark-films/260926-2.filA"
  "$ROOT/tests/benchmark-films/260926-3.filA"
)
NAMES=(L00-demo L06-combat net-5D-space own-260926-1 own-260926-2 own-260926-3)
if [[ $GAME != m2 ]]; then
  # Other games: their demo films (Marathon 1 has none: its first test film)
  # and any benchmark films recorded for them in tests/benchmark-films/<game>.
  FILMS=("$GAME_DATA"/Demos/*.filA(N) "$ROOT/tests/benchmark-films/$GAME"/*.filA(N))
  (( ${#FILMS} )) || FILMS=("$GAME_DEMO")
  NAMES=(); for f in $FILMS; do NAMES+=("$GAME-${${f:t:r}//[^A-Za-z0-9]/-}"); done
fi
# FILMS_ONLY="1 4 5" limits a run to those films (indices into FILMS)
OUT="$ROOT/.deps/bench/$LABEL"; mkdir -p "$OUT" "$ROOT/docs/benchmarks"
MD="$ROOT/docs/benchmarks/$LABEL.md"
{
  echo "# Benchmark: $LABEL"; echo
  echo "- Date: $(date '+%Y-%m-%d %H:%M')"
  echo "- Commit: $(git -C "$ROOT" rev-parse --short HEAD) ($(git -C "$ROOT" rev-parse --abbrev-ref HEAD))"
  echo "- Machine: $(sysctl -n hw.model), $(sysctl -n machdep.cpu.brand_string), $(( $(sysctl -n hw.memsize) / 1073741824 )) GB"
  echo "- Power: $(pmset -g batt | head -1 | sed "s/.*'\(.*\)'.*/\1/")"
  echo "- Settings: uncapped (fps target 0), sound off, ${DURANDAL_SET:+DURANDAL_QA=${DURANDAL_QA:-} DURANDAL_SET=$DURANDAL_SET}${DURANDAL_SET:+}"
  [[ -z ${DURANDAL_SET:-} ]] && echo "  (stock renderer settings)"
  echo
} > "$MD"
if [[ ${WARMUP:-1} == 1 ]]; then
  echo "Warm-up (not recorded) ..."
  DURANDAL_BENCHMARK="$OUT/warmup.csv" DURANDAL_BENCHMARK_FILM="${FILMS[1]}" DURANDAL_BENCHMARK_SIZE="${SIZES[1]}" \
    "$APP" -s --no-chooser > "$OUT/warmup.log" 2>&1 || true
  echo "- Warm-up: one untimed run of ${NAMES[1]} before measuring" >> "$MD"; echo >> "$MD"
fi
for size in $SIZES; do
  for i in ${=FILMS_ONLY:-{1..${#FILMS}}}; do
    name="${NAMES[$i]}-$size"
    echo "Running $name ..."
    therm_before="$(xcrun swift "$ROOT/scripts/thermal.swift" 2>/dev/null | grep -v warning); $(pmset -g therm | grep -i -E 'CPU_Speed_Limit' | tr '\n' ' ' || true); daemons: $(ps -Ao %cpu,comm -r | awk 'NR>1 && $1>10 {printf "%s %s%%; ", $2, $1}' | sed 's|.*/||')"
    # Settings go in the environment: macOS treats non-dash command-line
    # words as documents to open and shows an error alert.
    DURANDAL_BENCHMARK="$OUT/$name.csv" DURANDAL_BENCHMARK_FILM="${FILMS[$i]}" DURANDAL_BENCHMARK_SIZE="$size" \
      "$APP" -s --no-chooser > "$OUT/$name.log" 2>&1 || true
    { echo "## $name"; echo; echo '```';
      cat "$OUT/$name.csv.summary.txt" 2>/dev/null || echo "no summary (see $OUT/$name.log)"
      echo '```'; [[ -n "$therm_before" ]] && echo "Before the run: $therm_before"; echo; } >> "$MD"
  done
done
echo "Done -> $MD"
