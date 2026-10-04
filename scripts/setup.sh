#!/bin/zsh
# From a fresh clone to a game you can play, in one command:
#   1. the build dependencies (kept inside this folder; about 8 minutes, once)
#   2. the community HD art, fetched from its authors' own pages
#   3. the app, built and copied to the top of this folder
#
#   scripts/setup.sh                Durandal.app (Marathon 2; about 1 GB of art)
#   GAME=inf scripts/setup.sh       Durandal Infinity.app (about 1 GB of art)
#   GAME=m1 scripts/setup.sh        Durandal Marathon.app (about 0.1 GB of art)
#   GAME=all scripts/setup.sh       all three
#   scripts/setup.sh --no-art       without the HD art (the original art, enhanced)
#
# Needs Xcode, from the App Store, opened once so that it finishes installing.
# Safe to run again: each step is skipped where its work is already done.
set -euo pipefail
ROOT=${0:A:h:h}
cd "$ROOT"

if [[ ${GAME:-m2} == all ]]; then
  for g in m2 inf m1; do GAME=$g "$0" "$@"; done
  exit 0
fi
source "$ROOT/scripts/game.sh"   # GAME=m2|inf|m1

if ! xcodebuild -version > /dev/null 2>&1; then
  echo "Xcode is needed. Install it from the App Store, open it once, then run this again."
  exit 1
fi

if [[ ! -e "$GAME_DATA/${GAME_DATA_FILES[1]}" ]]; then
  echo "== The game data (Bungie's, from Aleph One's repository)"
  git submodule update --init "data/Scenarios/$GAME_SCENARIO"
fi

if [[ ! -d vcpkg/installed-arm64-osx ]]; then
  echo "== The build dependencies (about 8 minutes, once)"
  scripts/install-deps.sh
fi

if [[ "${1:-}" != "--no-art" ]]; then
  echo "== The HD art for $GAME_TITLE, from its authors' pages"
  scripts/get-hd-art.sh || echo "Some packs did not arrive. The game runs without them; run GAME=$GAME scripts/get-hd-art.sh again later, or use GET HD ART... in the game."
fi

echo "== $GAME_APP_NAME.app"
scripts/build.sh Release
rm -rf "$ROOT/$GAME_APP_NAME.app"
cp -R "$ROOT/.deps/DerivedData/Build/Products/Release/$GAME_APP_NAME.app" "$ROOT/$GAME_APP_NAME.app"

echo
echo "Done. Double-click $GAME_APP_NAME.app in this folder, or drag it to Applications."
echo "It starts on the Flagship tier. Too slow? Preferences > DURANDAL > Quality."
