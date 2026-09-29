#!/bin/zsh
# Gameplay-integrity gate: replays every Marathon 2 test film headless and
# checks the final game RNG seed matches the one recorded in the file name.
# Any change that alters game state (physics, RNG use, tick order) fails it.
# Runs with SDL's dummy video/audio drivers, so no window appears.
# Note: the test app writes its own prefs to
#   ~/Library/Preferences/org.bungie.source.AlephOneTests and
#   ~/Library/Application Support/AlephOne (created on first run).
set -euo pipefail
ROOT=${0:A:h:h}
CONFIG=${1:-Release}
"$ROOT/scripts/build.sh" "$CONFIG" tests
T="$ROOT/.deps/DerivedData/Build/Products/$CONFIG/AlephOne Tests.app/Contents/MacOS/AlephOne Tests"
REPLAYS="$ROOT/.deps/test-replays"
rm -rf "$REPLAYS"; mkdir -p "$REPLAYS"
rsync -a "$ROOT/tests/replays/Marathon 2/" "$REPLAYS/"
cd "$ROOT"
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy "$T" -g -s -Q \
  "data/Scenarios/Marathon 2" -l "$REPLAYS" 2>&1 | tail -4
