#!/bin/zsh
# From a fresh clone to a game you can play, in one command:
#   1. the build dependencies (kept inside this folder; about 8 minutes, once)
#   2. the community HD art, fetched from its authors' own pages (about 1 GB)
#   3. Durandal.app, built and copied to the top of this folder
#
#   scripts/setup.sh            everything
#   scripts/setup.sh --no-art   without the HD art (the original art, enhanced)
#
# Needs Xcode, from the App Store, opened once so that it finishes installing.
# Safe to run again: each step is skipped where its work is already done.
set -euo pipefail
ROOT=${0:A:h:h}
cd "$ROOT"

if ! xcodebuild -version > /dev/null 2>&1; then
  echo "Xcode is needed. Install it from the App Store, open it once, then run this again."
  exit 1
fi

if [[ ! -f "data/Scenarios/Marathon 2/Map.sceA" ]]; then
  echo "== The game data (Bungie's, from Aleph One's repository)"
  git submodule update --init "data/Scenarios/Marathon 2"
fi

if [[ ! -d vcpkg/installed-arm64-osx ]]; then
  echo "== The build dependencies (about 8 minutes, once)"
  scripts/install-deps.sh
fi

if [[ "${1:-}" != "--no-art" ]]; then
  echo "== The HD art, from its authors' pages"
  scripts/get-hd-art.sh || echo "Some packs did not arrive. The game runs without them; run scripts/get-hd-art.sh again later."
fi

echo "== Durandal.app"
scripts/build.sh Release
rm -rf "$ROOT/Durandal.app"
cp -R "$ROOT/.deps/DerivedData/Build/Products/Release/Durandal.app" "$ROOT/Durandal.app"

echo
echo "Done. Double-click Durandal.app in this folder, or drag it to Applications."
echo "It starts on the Flagship tier. Too slow? Preferences > DURANDAL > Quality."
