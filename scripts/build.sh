#!/bin/zsh
# Command-line build of a game's app (and optionally the film tests).
# Uses its own DerivedData in .deps so it never collides with Xcode's Run.
#   scripts/build.sh            -> Marathon 2 (Durandal.app), Release
#   scripts/build.sh Debug      -> Marathon 2, Debug
#   scripts/build.sh Release tests -> also builds "Aleph One Tests"
#   GAME=inf scripts/build.sh   -> Durandal Infinity.app; GAME=m1 Durandal Marathon.app
#   GAME=all scripts/build.sh   -> all three apps
set -euo pipefail
ROOT=${0:A:h:h}
CONFIG=${1:-Release}
DD="$ROOT/.deps/DerivedData"
LOG="$ROOT/.deps/build-$CONFIG.log"
if [[ ${GAME:-m2} == all ]]; then
  schemes=("Marathon 2" "Marathon 3" "Marathon 1")
else
  source "$ROOT/scripts/game.sh"
  schemes=("$GAME_SCHEME")
fi
[[ ${2:-} == tests ]] && schemes+=("Aleph One Tests")
for s in $schemes; do
  echo "Building $s ($CONFIG)..."
  if ! xcodebuild -project "$ROOT/Xcode/AlephOne.xcodeproj" -scheme "$s" \
      -configuration "$CONFIG" -destination 'platform=macOS,arch=arm64' \
      -derivedDataPath "$DD" build > "$LOG" 2>&1; then
    grep -E 'error:' "$LOG" | sort -u | head -30
    echo "BUILD FAILED ($s) - full log: $LOG"; exit 1
  fi
done
echo "BUILD SUCCEEDED -> $DD/Build/Products/$CONFIG"
