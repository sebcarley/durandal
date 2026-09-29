#!/bin/zsh
# Command-line build of the Marathon 2 app (and optionally the film tests).
# Uses its own DerivedData in .deps so it never collides with Xcode's Run.
#   scripts/build.sh            -> Marathon 2, Release
#   scripts/build.sh Debug      -> Marathon 2, Debug
#   scripts/build.sh Release tests -> also builds "Aleph One Tests"
set -euo pipefail
ROOT=${0:A:h:h}
CONFIG=${1:-Release}
DD="$ROOT/.deps/DerivedData"
LOG="$ROOT/.deps/build-$CONFIG.log"
schemes=("Marathon 2")
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
