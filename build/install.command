#!/bin/bash
# minColor installer (macOS). Payload -> /Users/Shared/minColor ; panel -> newest AE's ScriptUI Panels.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
OUT="$HERE/../dist-panel"
[ -d "$OUT" ] || { echo "run build.py first"; exit 1; }
# dev install = exactly what the README tells users: copy panel + data folder into ScriptUI Panels
AE=$(ls -d "/Applications/Adobe After Effects"* 2>/dev/null | sort | tail -1)
[ -n "$AE" ] || { echo "After Effects not found"; exit 1; }
VER=$(ls "$HOME/Library/Preferences/Adobe/After Effects/" | sort -V | tail -1)
PANELS="$HOME/Library/Preferences/Adobe/After Effects/$VER/Scripts/ScriptUI Panels"
mkdir -p "$PANELS"
cp "$OUT/minColor.jsx" "$PANELS/"
rm -rf "$PANELS/minColor-data"
cp -R "$OUT/minColor-data" "$PANELS/"
echo "installed -> $PANELS (minColor.jsx + minColor-data)"
echo "Restart After Effects; the panel appears in the Window menu."
