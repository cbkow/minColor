#!/bin/bash
# minColor 2.0 dev install — the pair is ATOMIC: effect -> MediaCore, ceremonies AEGP ->
# the app's Plug-ins folder (admin). Installing one without the other recreates the version
# skew class that broke 0.9.x (PREPLAN §7). Real copies, never symlinks (AE won't register
# effects from a symlinked bundle). Refuses to run while AE is up (bundles are mmapped).
#
#   dev-install-both.sh [--ae-version 2026] [--remove-aegp]
#
# --remove-aegp: uninstall the AEGP (and any probe leftovers) — used by the baseline runner
#                to produce a pure single-bundle environment.
set -e
HERE="$(cd "$(dirname "$0")/.." && pwd)"
AE_VER="2026"
REMOVE_AEGP=0
while [ $# -gt 0 ]; do
  case "$1" in
    --ae-version) AE_VER="$2"; shift 2 ;;
    --remove-aegp) REMOVE_AEGP=1; shift ;;
    *) echo "unknown arg: $1"; exit 1 ;;
  esac
done

if osascript -e 'tell application "System Events" to (name of processes) contains "After Effects '"$AE_VER"'"' | grep -q true; then
  echo "After Effects $AE_VER is running — quit it first."; exit 1
fi

MEDIACORE="/Library/Application Support/Adobe/Common/Plug-ins/7.0/MediaCore/minColor"
PLUGINS="/Applications/Adobe After Effects $AE_VER/Plug-ins"
STAGE="/Users/Shared/minColor-staging"   # NOT /Users/Shared/minColor — that's the panel's SHARED_ROOT

if [ "$REMOVE_AEGP" = "1" ]; then
  osascript -e 'do shell script "rm -rf \"'"$PLUGINS"'/minColorAEGP.plugin\" \"'"$PLUGINS"'/mincProbeAegp.plugin\"" with administrator privileges'
  echo "removed AEGP (and probe leftovers) from $PLUGINS"
  exit 0
fi

EFFECT="$HERE/build/minColorCST.plugin"
AEGP="$HERE/build/minColorAEGP.plugin"
[ -d "$EFFECT" ] && [ -d "$AEGP" ] || { echo "build first (cmake --build plugin/build)"; exit 1; }

# same-build guard: both bundles must carry the identical stamp
S1=$(strings "$EFFECT/Contents/MacOS/minColorCST" | grep -m1 "2\.0\.0")
S2=$(strings "$AEGP/Contents/MacOS/minColorAEGP" | grep -m1 "2\.0\.0")
[ -n "$S1" ] && [ "$S1" = "$S2" ] || { echo "build stamp mismatch: '$S1' vs '$S2' — rebuild both"; exit 1; }

# effect -> MediaCore
if [ -w "$MEDIACORE" ]; then
  rm -rf "$MEDIACORE/minColorCST.plugin"
  cp -R "$EFFECT" "$MEDIACORE/minColorCST.plugin"
else
  sudo mkdir -p "$MEDIACORE"
  sudo rm -rf "$MEDIACORE/minColorCST.plugin"
  sudo cp -R "$EFFECT" "$MEDIACORE/minColorCST.plugin"
fi
echo "effect -> $MEDIACORE/minColorCST.plugin"

# AEGP -> app Plug-ins (admin; stage via /Users/Shared — TCC blocks root reading ~/Documents)
rm -rf "$STAGE"; mkdir -p "$STAGE"
cp -R "$AEGP" "$STAGE/minColorAEGP.plugin"
osascript -e 'do shell script "rm -rf \"'"$PLUGINS"'/minColorAEGP.plugin\" \"'"$PLUGINS"'/mincProbeAegp.plugin\" && cp -R \"'"$STAGE"'/minColorAEGP.plugin\" \"'"$PLUGINS"'/\" && rm -rf \"'"$STAGE"'\"" with administrator privileges'
echo "AEGP  -> $PLUGINS/minColorAEGP.plugin"
echo "installed pair: $S1 (restart AE)"
