#!/bin/zsh
# Bootstraps a private vcpkg in .deps/vcpkg and installs the arm64 macOS
# dependencies exactly as vcpkg/install-arm-osx.sh does, without
# `vcpkg integrate install` (which writes to ~/.vcpkg).
set -euo pipefail
ROOT=${0:A:h:h}
VCPKG="$ROOT/.deps/vcpkg"
if [[ ! -x "$VCPKG/vcpkg" ]]; then
  mkdir -p "$ROOT/.deps"
  [[ -d "$VCPKG" ]] || git clone https://github.com/microsoft/vcpkg "$VCPKG"
  "$VCPKG/bootstrap-vcpkg.sh" -disableMetrics
fi
cd "$ROOT/vcpkg"
"$VCPKG/vcpkg" --overlay-triplets=custom-triplets --triplet=arm64-osx \
  --x-install-root=installed-arm64-osx --feature-flags="versions" \
  --disable-metrics install
