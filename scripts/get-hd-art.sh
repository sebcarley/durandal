#!/bin/zsh
# Fetches the community HD art Durandal is played with and installs it as
# plugins. Nothing here is ours to distribute: every pack comes from its
# authors' own page on Simplici7y, by the download link they publish, and is
# theirs. Credits and versions: docs/HD_ASSETS.md.
#
#   scripts/get-hd-art.sh              Marathon 2's five packs (about 1.0 GB)
#   scripts/get-hd-art.sh scenery 3d   only the named ones
#                                      (walls monsters scenery weapons 3d)
#   GAME=inf scripts/get-hd-art.sh     Marathon Infinity's set (about 1.0 GB)
#
# Installs into the game's own Plugins folder (~/Library/Application
# Support/Durandal/Plugins, or "Durandal Infinity"), or $DURANDAL_PLUGINS_DIR.
# A pack that is already there is left alone: nothing is ever overwritten.
# Safe to run again after a failed download.
set -uo pipefail
ROOT=${0:A:h:h}
source "$ROOT/scripts/game.sh"   # GAME=m2|inf|m1

DEST=${DURANDAL_PLUGINS_DIR:-"$HOME/Library/Application Support/$GAME_APP_NAME/Plugins"}
WORK=$(mktemp -d "${TMPDIR:-/tmp}/durandal-hd-art.XXXXXX")
trap 'rm -rf "$WORK"' EXIT

# key | installed folder | Simplici7y item | megabytes | SHA-256 of the version tested
# (keep in step with Misc/DurandalFetch.mm)
case $GAME in
  m2) PACKS=(
  "walls|CFP - Walls M2|community-freeverse-plugin-walls-m2|373|3848f883e92d5fd27c616f5f85df5dbe6efdbbfac4be81d296062cf2d69094b3"
  "monsters|CFP Monsters|community-freeverse-plugin-monsters|516|94bf18219cb9132428da60f85cbfd64b56ad72bdde3dcad04741ee1cc12d4ece"
  "scenery|CFP Scenery|community-freeverse-plugin-scenery|24|aea1b1106c94b3d1462900d99fcecac07cc93593197a13766d5f65e8910ff770"
  "weapons|CFP Weapons M2|community-freeverse-plugin-weapons-m2|66|3354fc30f21892aef33c9f0f5a6fa8fd8b718e1b46553c5b59824a654633a7a0"
  "3d|3D Items|3d-items-plugin|3|b4673ac3d6b43f4beb4bb629772f50e64e02d3ad97859a77e5bc9386684b3f99"
  ) ;;
  inf) PACKS=(
  "walls|CFP - Walls MInf|communityfreeverse-walls-minf|425|"
  "monsters|CFP Monsters|community-freeverse-plugin-monsters|516|94bf18219cb9132428da60f85cbfd64b56ad72bdde3dcad04741ee1cc12d4ece"
  "scenery|CFP Scenery|community-freeverse-plugin-scenery|24|aea1b1106c94b3d1462900d99fcecac07cc93593197a13766d5f65e8910ff770"
  "weapons|CFP Weapons MInf|community-freeverse-plugin-weapons|73|"
  "3d|3D Items|3d-items-plugin|3|b4673ac3d6b43f4beb4bb629772f50e64e02d3ad97859a77e5bc9386684b3f99"
  ) ;;
  *) echo "No HD art list for $GAME_TITLE yet."; exit 1 ;;
esac

want=("$@")
mkdir -p "$DEST" || { echo "Cannot create $DEST"; exit 1; }
installed=0; skipped=0; failed=0

for pack in $PACKS; do
  key=${pack%%|*}; rest=${pack#*|}
  name=${rest%%|*}; rest=${rest#*|}
  item=${rest%%|*}; rest=${rest#*|}
  size=${rest%%|*}; sum=${rest#*|}
  if (( ${#want} )) && (( ! ${want[(Ie)$key]} )); then continue; fi

  if [[ -e "$DEST/$name" || -L "$DEST/$name" ]]; then
    echo "$name: already installed, left alone"
    skipped=$((skipped + 1)); continue
  fi

  echo "$name: finding the authors' download (simplici7y.com/items/$item)"
  link=$(curl -sI -m 30 "https://simplici7y.com/items/$item/downloads/new" | tr -d '\r' | awk 'tolower($1) == "location:" { print $2 }' | tail -1)
  if [[ -z "$link" ]]; then
    echo "$name: no download link came back; get it by hand from https://simplici7y.com/items/$item/"
    failed=$((failed + 1)); continue
  fi
  # A Google Drive page becomes the file behind it
  if [[ "$link" == *drive.google.com* ]]; then
    id=$(print -r -- "$link" | sed -E 's#.*/file/d/([^/?]+).*#\1#; s#.*[?&]id=([^&]+).*#\1#')
    link="https://drive.usercontent.google.com/download?id=$id&export=download&confirm=t"
  fi

  zip="$WORK/$key.zip"
  echo "$name: downloading about $size MB"
  if ! curl -L --fail --progress-bar -m 3600 -o "$zip" "$link"; then
    echo "$name: the download failed; get it by hand from https://simplici7y.com/items/$item/"
    failed=$((failed + 1)); continue
  fi
  if ! unzip -tq "$zip" > /dev/null 2>&1; then
    echo "$name: what arrived is not a zip archive (the host may want a browser); get it by hand from https://simplici7y.com/items/$item/"
    failed=$((failed + 1)); continue
  fi
  got=$(shasum -a 256 "$zip" | awk '{ print $1 }')
  echo "$name: SHA-256 $got"
  if [[ -n "$sum" && "$got" != "$sum" ]]; then
    echo "$name: a different version from the one Durandal was tested with (its authors have updated it). Installing it all the same."
  fi

  out="$WORK/$key"
  mkdir -p "$out"
  unzip -q "$zip" -d "$out" 2> /dev/null || { echo "$name: could not unpack"; failed=$((failed + 1)); continue; }
  rm -rf "$out/__MACOSX"
  # The plugin is the folder holding Plugin.xml: the archive's root, or a folder inside it
  manifest=$(find "$out" -name Plugin.xml -not -path '*/__MACOSX/*' | awk '{ print length($0), $0 }' | sort -n | head -1 | cut -d' ' -f2-)
  if [[ -z "$manifest" ]]; then
    echo "$name: no Plugin.xml inside; not installed"
    failed=$((failed + 1)); continue
  fi
  if ! mv "${manifest:h}" "$DEST/$name"; then
    echo "$name: could not move it into $DEST"
    failed=$((failed + 1)); continue
  fi
  echo "$name: installed"
  installed=$((installed + 1))
  rm -f "$zip"
done

echo
echo "$installed installed, $skipped already there, $failed failed. Plugins folder: $DEST"
echo "In the game: Preferences > DURANDAL > ART shows what is installed. HD art applies from the next level."
(( failed == 0 ))
