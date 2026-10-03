#!/bin/zsh
# Builds Durandal.app for other people's Macs: Developer ID signed,
# notarised by Apple and stapled, so it opens with a double-click and no
# warning. The zip and its checksum land in .deps/release/<version>/, ready
# to attach to a GitHub Release.
#
#   scripts/make-release.sh 0.1.0                       sign, notarise, staple, zip
#   RELEASE_NOTARISE=0 scripts/make-release.sh 0.1.0    sign and zip, no Apple round trip
#   RELEASE_SIGN=0 scripts/make-release.sh 0.1.0        ad-hoc signed only (a dry run)
#
# Notarisation uses a keychain profile the owner stores once:
#   xcrun notarytool store-credentials durandal
# (RELEASE_NOTARY_PROFILE names another). The Developer ID certificate is
# found in the keychain by name, never written here.
#
# The app is self-contained: the vcpkg dependencies are linked statically
# and Bungie's freely released Marathon 2 data is bundled
# (Contents/Resources/DataFiles). The community HD art is not in it: the
# game's GET HD ART... button fetches it from its authors.
set -euo pipefail
ROOT=${0:A:h:h}
VERSION=${1:?usage: scripts/make-release.sh <version, e.g. 0.1.0>}
OUT="$ROOT/.deps/release/$VERSION"
APP="$OUT/Durandal.app"
ZIP="$OUT/Durandal-$VERSION.zip"
PROFILE=${RELEASE_NOTARY_PROFILE:-durandal}

cd "$ROOT"
if [[ -n $(git status --porcelain --untracked-files=no) && ${RELEASE_DIRTY:-0} != 1 ]]; then
  echo "FAIL: uncommitted changes; a release is built from a commit (RELEASE_DIRTY=1 overrides)"; exit 1
fi
COMMIT=$(git rev-parse --short HEAD)

scripts/build.sh Release
rm -rf "$OUT" && mkdir -p "$OUT"
ditto "$ROOT/.deps/DerivedData/Build/Products/Release/Durandal.app" "$APP"

# --- the bundle must run on a Mac without this folder ----------------------
echo "== dependency check"
LEAK=$(otool -L "$APP/Contents/MacOS/Durandal" | tail -n +2 | grep -v -E "^\s*(/System/|/usr/lib/)" || true)
if [[ -n $LEAK ]]; then
  echo "$LEAK"; echo "FAIL: the binary links libraries outside macOS"; exit 1
fi
for f in Map.sceA Shapes.shpA Sounds.sndA Images.imgA "Physics Models"; do
  [[ -e "$APP/Contents/Resources/DataFiles/$f" ]] || { echo "FAIL: game data missing: $f"; exit 1; }
done
echo "   clean: system frameworks only; the Marathon 2 data is bundled"

# --- sign ---------------------------------------------------------------------
# Inside out, never --deep (it re-signs nested code with the outer options);
# hardened runtime and a secure timestamp, as notarisation requires; the
# entitlements the build gave the app (microphone, for net game voice) kept.
SIGNED=adhoc
if [[ ${RELEASE_SIGN:-1} == 1 ]]; then
  SIGN_ID=$(security find-identity -v -p codesigning | awk -F'"' '/Developer ID Application/{print $2; exit}')
  [[ -n $SIGN_ID ]] || { echo "FAIL: no Developer ID Application certificate in the keychain"; exit 1; }
  echo "== signing with the Developer ID certificate (team ${SIGN_ID##*\(}"
  ENT="$OUT/entitlements.plist"
  codesign -d --entitlements - --xml "$APP" > "$ENT" 2>/dev/null
  find "$APP/Contents" \( -name "*.dylib" -o -name "*.framework" \) -print0 2>/dev/null |
    while IFS= read -r -d '' f; do codesign --force --options runtime --timestamp --sign "$SIGN_ID" "$f"; done
  codesign --force --options runtime --timestamp --entitlements "$ENT" --sign "$SIGN_ID" "$APP"
  rm -f "$ENT"
  codesign --verify --deep --strict "$APP" || { echo "FAIL: the signature does not verify"; exit 1; }
  SIGNED=developerid

  if [[ ${RELEASE_NOTARISE:-1} == 1 ]]; then
    echo "== notarising (Apple usually answers in a few minutes)"
    NZ=$(mktemp -d "${TMPDIR:-/tmp}/durandal-notary.XXXXXX")
    ditto -c -k --keepParent "$APP" "$NZ/app.zip"
    xcrun notarytool submit "$NZ/app.zip" --keychain-profile "$PROFILE" --wait > "$NZ/log" 2>&1 || true
    tail -8 "$NZ/log"
    if ! grep -q "status: Accepted" "$NZ/log"; then
      SUB=$(awk '/^ *id:/{print $2; exit}' "$NZ/log")
      [[ -n $SUB ]] && xcrun notarytool log "$SUB" --keychain-profile "$PROFILE" 2>&1 | tail -40
      echo "FAIL: Apple did not accept the app (its answer is above; HTTP 403 \"agreement\": the account holder must accept Apple's updated agreement at developer.apple.com/account; no profile: xcrun notarytool store-credentials $PROFILE)"; exit 1
    fi
    rm -rf "$NZ"
    xcrun stapler staple "$APP" || { echo "FAIL: could not staple the ticket"; exit 1; }
    spctl -a -vv -t exec "$APP" 2>&1 | sed 's/^/   /'
    spctl -a -t exec "$APP" || { echo "FAIL: Gatekeeper rejects the app"; exit 1; }
    SIGNED=notarised
  fi
else
  codesign --force --sign - "$APP"
fi

# --- zip ----------------------------------------------------------------------
# ditto, so the signature and the stapled ticket survive
ditto -c -k --keepParent "$APP" "$ZIP"
(cd "$OUT" && shasum -a 256 "${ZIP:t}" > "${ZIP:t}.sha256")
printf '%s  %s  %s\n' "$VERSION" "$COMMIT" "$SIGNED" > "$OUT/BUILD.txt"
echo "== $SIGNED: $ZIP ($(du -h "$ZIP" | cut -f1))"
