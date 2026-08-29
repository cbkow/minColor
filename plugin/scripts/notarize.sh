#!/bin/bash
# Sign the mac plugin with Developer ID, notarize it, staple the ticket. Run after `cmake --build`
# and BEFORE build/build.py, so the release zip carries a bundle that loads straight from a download.
#
# One-time: store notarization credentials (Apple ID + app-specific password, team 5Z4S9VHV56):
#   xcrun notarytool store-credentials AC_PASSWORD --apple-id you@example.com --team-id 5Z4S9VHV56   (QCView-Player already did this)
# Then:  plugin/scripts/notarize.sh ["Developer ID Application: Name (TEAMID)"]
set -e
HERE="$(cd "$(dirname "$0")/.." && pwd)"
PLUGIN="$HERE/build/minColorCST.plugin"
IDENTITY="${1:-$(security find-identity -v -p codesigning | grep -o '"Developer ID Application: [^"]*"' | head -1 | tr -d '"')}"
PROFILE="${NOTARY_PROFILE:-AC_PASSWORD}"   # same keychain profile QCView-Player uses
[ -d "$PLUGIN" ] || { echo "build first (cmake --build plugin/build)"; exit 1; }
[ -n "$IDENTITY" ] || { echo "no Developer ID Application identity found"; exit 1; }
echo "signing with: $IDENTITY"
codesign --force --deep --options runtime --timestamp --sign "$IDENTITY" "$PLUGIN"
codesign --verify --deep --strict --verbose=2 "$PLUGIN"
TMP="$(mktemp -d)"; ZIP="$TMP/minColorCST.zip"
ditto -c -k --keepParent "$PLUGIN" "$ZIP"
echo "submitting to Apple notary service (profile '$PROFILE')…"
xcrun notarytool submit "$ZIP" --keychain-profile "$PROFILE" --wait
xcrun stapler staple "$PLUGIN"
xcrun stapler validate "$PLUGIN"
# (spctl --type execute only judges apps; for a plugin bundle the stapled ticket + codesign verify above
#  is the proof — a quarantined copy loads in AE without the "damaged" verdict.)
rm -rf "$TMP"
echo "done — now run build/build.py for the release zip"
