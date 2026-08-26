#!/bin/bash
# minColor installer (macOS). Payload -> /Users/Shared/minColor ; panel -> newest AE's ScriptUI Panels.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
OUT="$HERE/../dist-panel"
[ -d "$OUT" ] || { echo "run build.py first"; exit 1; }
DEST="/Users/Shared/minColor"
mkdir -p "$DEST/configs" "$DEST/settings"
# configs are append-only: never delete old hashed versions (projects pin them)
cp -Rn "$OUT/payload/configs/." "$DEST/configs/" 2>/dev/null || true
cp -R "$OUT/payload/configs/luts" "$OUT/payload/configs/filmic" "$OUT/payload/configs/icc" "$DEST/configs/" 2>/dev/null || true
cp "$OUT"/payload/configs/*.ocio "$OUT/payload/configs/presets.json" "$DEST/configs/" 2>/dev/null || true
[ -f "$DEST/settings/extension-defaults.json" ] || cp "$OUT/payload/settings/extension-defaults.json" "$DEST/settings/"
AE=$(ls -d "/Applications/Adobe After Effects"* 2>/dev/null | sort | tail -1)
[ -n "$AE" ] || { echo "After Effects not found"; exit 1; }
# user preferences panels folder (what AE's own "Install ScriptUI Panel" uses) — no admin rights
VER=$(ls "$HOME/Library/Preferences/Adobe/After Effects/" | sort -V | tail -1)
PANELS="$HOME/Library/Preferences/Adobe/After Effects/$VER/Scripts/ScriptUI Panels"
mkdir -p "$PANELS"
cp "$OUT/minColor.jsx" "$PANELS/"
echo "panel installed -> $PANELS"
echo "payload installed -> $DEST"
echo "Restart After Effects; the panel appears in the Window menu."
