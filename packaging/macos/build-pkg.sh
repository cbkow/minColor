#!/bin/bash
# minColor macOS installer: one .pkg that places the (notarized) plug-in + config store into MediaCore,
# the panel into every installed After Effects >= 2025 for the console user, and seeds the shared
# settings — merging append-only (never deletes a hashed config, never overwrites user settings).
#
#   packaging/macos/build-pkg.sh            -> dist-panel/minColor-<ver>.pkg (signed + notarized when
#                                              a "Developer ID Installer" identity exists; else unsigned)
# Prereqs: cmake --build plugin/build && plugin/scripts/notarize.sh && python3 build/build.py
set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
DIST="$ROOT/dist-panel"; OUT="$ROOT/dist-panel"
VER="$(grep -o 'var VERSION = "[^"]*"' "$ROOT/src/minColor.jsxinc" | sed 's/.*"\(.*\)"/\1/')"
PLUGIN="$ROOT/plugin/build/minColorCST.plugin"
[ -d "$PLUGIN" ] || { echo "build the plug-in first"; exit 1; }
[ -f "$DIST/minColor.jsx" ] || { echo "run build/build.py first"; exit 1; }
xcrun stapler validate "$PLUGIN" >/dev/null 2>&1 || echo "WARNING: plug-in is not notarized/stapled (run plugin/scripts/notarize.sh) — the pkg will still build"
STAGE="$(mktemp -d)"; trap 'rm -rf "$STAGE"' EXIT
# ---- payload root (absolute locations) ----
MC="$STAGE/root/Library/Application Support/Adobe/Common/Plug-ins/7.0/MediaCore/minColor"
mkdir -p "$MC"
ditto "$PLUGIN" "$MC/minColorCST.plugin"
ditto "$ROOT/config/dist" "$MC/configs"
# panel + settings seed staged under the shared store; postinstall fans the panel out per AE version
SH="$STAGE/root/Users/Shared/minColor"
mkdir -p "$SH/panel" "$SH/configs" "$SH/settings-seed"
cp "$DIST/minColor.jsx" "$SH/panel/"; ditto "$DIST/minColor-data" "$SH/panel/minColor-data"
ditto "$ROOT/config/dist" "$SH/configs"
cp "$ROOT/config/extension-defaults.json" "$ROOT/config/render-presets.json" "$SH/settings-seed/"
# ---- component pkg ----
mkdir -p "$STAGE/pkgs"
pkgbuild --analyze --root "$STAGE/root" "$STAGE/components.plist" >/dev/null
/usr/libexec/PlistBuddy -c "Set :0:BundleIsRelocatable false" "$STAGE/components.plist" 2>/dev/null || true
pkgbuild --root "$STAGE/root" --component-plist "$STAGE/components.plist" \
         --identifier ski.bialkow.minColor --version "$VER" --install-location / \
         --scripts "$ROOT/packaging/macos/scripts" "$STAGE/pkgs/minColor-core.pkg" >/dev/null
# ---- distribution (title, AE-running check, requirements) ----
sed "s/@VERSION@/$VER/g" "$ROOT/packaging/macos/distribution.xml" > "$STAGE/distribution.xml"
INSTALLER_ID="$(security find-identity -v 2>/dev/null | grep -o '"Developer ID Installer: [^"]*"' | head -1 | tr -d '"')"
PKG="$OUT/minColor-$VER.pkg"
if [ -n "$INSTALLER_ID" ]; then
  productbuild --distribution "$STAGE/distribution.xml" --package-path "$STAGE/pkgs" --sign "$INSTALLER_ID" --timestamp "$PKG" >/dev/null
  echo "signed with: $INSTALLER_ID"
  PROFILE="${NOTARY_PROFILE:-AC_PASSWORD}"
  xcrun notarytool submit "$PKG" --keychain-profile "$PROFILE" --wait | tail -2
  xcrun stapler staple "$PKG" && xcrun stapler validate "$PKG" | tail -1
  spctl --assess --type install --verbose=2 "$PKG" 2>&1 | tail -1
else
  productbuild --distribution "$STAGE/distribution.xml" --package-path "$STAGE/pkgs" "$PKG" >/dev/null
  echo "UNSIGNED (no 'Developer ID Installer' identity in the keychain) — fine for local testing only"
fi
echo "-> $PKG ($(du -h "$PKG" | cut -f1))"
