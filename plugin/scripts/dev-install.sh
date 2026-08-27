#!/bin/bash
# Copy the built .plugin into MediaCore. MUST be a real copy: AE loads the dylib from a
# symlinked bundle but never REGISTERS its effects (verified 2026-08-28 — effect absent
# from app.effects, GLOBAL_SETUP never runs). Dir is admin-writable after first install.
set -e
HERE="$(cd "$(dirname "$0")/.." && pwd)"
DEST="/Library/Application Support/Adobe/Common/Plug-ins/7.0/MediaCore/minColor"
PLUGIN="$HERE/build/minColorCST.plugin"
[ -d "$PLUGIN" ] || { echo "build first (cmake --build plugin/build)"; exit 1; }
if [ -w "$DEST" ]; then
  rm -rf "$DEST/minColorCST.plugin"
  cp -R "$PLUGIN" "$DEST/minColorCST.plugin"
else
  sudo mkdir -p "$DEST"
  sudo rm -rf "$DEST/minColorCST.plugin"
  sudo cp -R "$PLUGIN" "$DEST/minColorCST.plugin"
fi
echo "copied -> $DEST/minColorCST.plugin (restart AE)"
