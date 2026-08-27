#!/bin/bash
# Symlink the built .plugin into MediaCore for iteration (sudo needed once for the dir).
set -e
HERE="$(cd "$(dirname "$0")/.." && pwd)"
DEST="/Library/Application Support/Adobe/Common/Plug-ins/7.0/MediaCore/minColor"
PLUGIN="$HERE/build/minColorCST.plugin"
[ -d "$PLUGIN" ] || { echo "build first (cmake --build plugin/build)"; exit 1; }
sudo mkdir -p "$DEST"
sudo ln -sfn "$PLUGIN" "$DEST/minColorCST.plugin"
echo "linked -> $DEST/minColorCST.plugin (restart AE)"
