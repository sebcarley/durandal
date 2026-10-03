#!/bin/zsh
# Gameplay-integrity gate: replays every test film of a game headless and
# checks the final game RNG seed matches the one recorded in the file name.
# Any change that alters game state (physics, RNG use, tick order) fails it.
# Runs with SDL's dummy video/audio drivers, so no window appears.
#   scripts/test-films.sh           Marathon 2 (42 films, 86 assertions)
#   GAME=inf scripts/test-films.sh  Marathon Infinity (32 films)
#   GAME=m1 scripts/test-films.sh   Marathon (27 films)
#   GAME=all scripts/test-films.sh  all three, one after another
# Note: the test app writes its own prefs to
#   ~/Library/Preferences/org.bungie.source.AlephOneTests and
#   ~/Library/Application Support/AlephOne (created on first run).
set -euo pipefail
ROOT=${0:A:h:h}
CONFIG=${1:-Release}
if [[ ${GAME:-m2} == all ]]; then
  failed=()
  for g in m2 inf m1; do
    echo "== $g"
    GAME=$g SKIP_BUILD=${SKIP_BUILD:-0} "$0" "$CONFIG" || failed+=($g)
    SKIP_BUILD=1
  done
  [[ -z $failed ]] || { echo "FAILED: $failed"; exit 1; }
  exit 0
fi
source "$ROOT/scripts/game.sh"
# The tests app is shared by the three games; only the data and films differ.
[[ ${SKIP_BUILD:-0} == 1 ]] || GAME=m2 "$ROOT/scripts/build.sh" "$CONFIG" tests
T="$ROOT/.deps/DerivedData/Build/Products/$CONFIG/AlephOne Tests.app/Contents/MacOS/AlephOne Tests"
REPLAYS="$ROOT/.deps/test-replays"
rm -rf "$REPLAYS"; mkdir -p "$REPLAYS"
rsync -a "$GAME_REPLAYS/" "$REPLAYS/"
cd "$ROOT"
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy "$T" -g -s -Q \
  "$GAME_DATA" -l "$REPLAYS" 2>&1 | tail -4
