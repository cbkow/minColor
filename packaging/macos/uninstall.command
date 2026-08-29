#!/bin/bash
# Removes the plug-in, the panel copies and the shared store. Keeps nothing — run only if you mean it.
echo "This removes minColor (plug-in, panel, shared store). Projects keep working only if a config store remains elsewhere."
read -p "Continue? [y/N] " a; [ "$a" = "y" ] || exit 0
sudo rm -rf "/Library/Application Support/Adobe/Common/Plug-ins/7.0/MediaCore/minColor"
rm -rf "$HOME/Library/Preferences/Adobe/After Effects/"*/"Scripts/ScriptUI Panels/minColor.jsx" "$HOME/Library/Preferences/Adobe/After Effects/"*/"Scripts/ScriptUI Panels/minColor-data"
sudo rm -rf /Users/Shared/minColor
sudo pkgutil --forget ski.bialkow.minColor 2>/dev/null
echo "removed"
