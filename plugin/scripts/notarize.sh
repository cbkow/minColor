#!/bin/bash
# Sign BOTH mac bundles (effect + AEGP) with Developer ID, notarize them, staple the tickets. Run
# after `cmake --build` and BEFORE build/build.py, so the release carries bundles that load straight
# from a download — AND so the .pkg's own notarization passes (it rejects any ad-hoc-signed binary
# in the payload; the AEGP used to ship ad-hoc, which would fail notarizing the .pkg).
#
# One-time: store notarization credentials in a notarytool keychain profile (Apple ID +
# app-specific password + team ID):
#   xcrun notarytool store-credentials <profile> --apple-id you@example.com --team-id <TEAMID>
# Then:  NOTARY_PROFILE=<profile> plugin/scripts/notarize.sh ["Developer ID Application: Name (TEAMID)"]
# (identity defaults to the first Developer ID Application identity; profile defaults to "notary")
set -e
HERE="$(cd "$(dirname "$0")/.." && pwd)"
EFFECT="$HERE/build/minColorCST.plugin"
AEGP="$HERE/build/minColorAEGP.plugin"
IDENTITY="${1:-$(security find-identity -v -p codesigning | grep -o '"Developer ID Application: [^"]*"' | head -1 | tr -d '"')}"
PROFILE="${NOTARY_PROFILE:-notary}"
[ -d "$EFFECT" ] && [ -d "$AEGP" ] || { echo "build both bundles first (cmake --build plugin/build)"; exit 1; }
[ -n "$IDENTITY" ] || { echo "no Developer ID Application identity found"; exit 1; }

echo "signing with: $IDENTITY"
for B in "$EFFECT" "$AEGP"; do
  codesign --force --deep --options runtime --timestamp --sign "$IDENTITY" "$B"
  codesign --verify --deep --strict --verbose=2 "$B"
done

# one submission for both bundles: a single zip carrying both, then staple each
TMP="$(mktemp -d)"; trap 'rm -rf "$TMP"' EXIT
ZIP="$TMP/minColor-bundles.zip"
mkdir -p "$TMP/b"
ditto "$EFFECT" "$TMP/b/minColorCST.plugin"
ditto "$AEGP"   "$TMP/b/minColorAEGP.plugin"
ditto -c -k "$TMP/b" "$ZIP"
echo "submitting both bundles to Apple notary service (profile '$PROFILE')…"
xcrun notarytool submit "$ZIP" --keychain-profile "$PROFILE" --wait
for B in "$EFFECT" "$AEGP"; do
  xcrun stapler staple "$B"
  xcrun stapler validate "$B"
done
# (spctl --type execute only judges apps; for a plugin bundle the stapled ticket + codesign verify
#  above is the proof — a quarantined copy loads in AE without the "damaged" verdict.)
echo "done — both bundles signed + notarized + stapled. Now run build/build.py, then build-pkg.sh"
