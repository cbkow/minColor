#!/bin/bash
# Baseline runner — the M0 preservation contract, executable.
#   run.sh --record   # run all scenarios, save outputs as golden/
#   run.sh --check    # run all scenarios, diff outputs against golden/
# Installing the build under test is NOT this script's job (dev-install*.sh / checklist).
# Orchestration: quit AE -> snapshot shared settings -> cold-launch AE with the driver ->
# poll for DONE -> quit AE -> restore settings -> record or diff.
set -e
BASE="$(cd "$(dirname "$0")" && pwd)"
MODE="${1:---check}"
AE_VER="2026"
AE_APP="Adobe After Effects $AE_VER"
OUT="$BASE/out/current"
SETTINGS="/Users/Shared/minColor/settings"
SETTINGS_BAK="$BASE/out/settings-snapshot"

ae_running() { osascript -e "tell application \"System Events\" to (name of processes) contains \"After Effects\"" ; }
ae_quit() {
  if [ "$(ae_running)" = "true" ]; then
    osascript -e "tell application \"$AE_APP\" to DoScript \"try{app.project.close(CloseOptions.DO_NOT_SAVE_CHANGES);}catch(e){}; app.quit();\"" >/dev/null 2>&1 || true
    for i in $(seq 1 30); do [ "$(ae_running)" = "false" ] && return 0; sleep 1; done
    echo "AE did not quit"; exit 1
  fi
}

rm -rf "$OUT"; mkdir -p "$OUT"
ae_quit
# settings snapshot (repair/heal and ui-state writes must not leak between runs)
rm -rf "$SETTINGS_BAK"
[ -d "$SETTINGS" ] && cp -R "$SETTINGS" "$SETTINGS_BAK" || true
# M1 quiet-mode: ceremonies must show no dialogs during automation (restore removes the marker)
mkdir -p "$SETTINGS"
touch "$SETTINGS/quiet-mode"
# pin the extension table: goldens were recorded against this exact table, and the user's
# live table (which the suite would otherwise read) legitimately drifts (container rules etc.)
cp "$BASE/fixtures/extension-defaults.suite.json" "$SETTINGS/extension-defaults.json"
# a docked dev shell initializes at AE startup and races the suite (shell-args writes) —
# move any aside for the run, restored with the settings below
PANELS_GLOB="$HOME/Library/Preferences/Adobe/After Effects"
for pd in "$PANELS_GLOB"/*/Scripts/ScriptUI\ Panels; do
  [ -f "$pd/minColor2 dev.jsx" ] && mv "$pd/minColor2 dev.jsx" "$pd/minColor2 dev.jsx.suite-stash" || true
done

# wrapper sets the globals then runs the driver (DoScript cannot pass arguments)
WRAP="$OUT/wrap.jsx"
cat > "$WRAP" << EOF
\$.global.__mincBase = "$BASE";
\$.global.__mincOut = "$OUT";
\$.evalFile("$BASE/lib/driver.jsx");
EOF

# NB quoting: bash \$ -> literal $ for AppleScript (AppleScript itself must see $.evalFile,
# never \$ — \$ is an invalid AppleScript escape and the launch fails silently)
osascript -e "tell application \"$AE_APP\" to DoScript \"try{\$.evalFile(\\\"$WRAP\\\")}catch(e){var f=new File(\\\"$OUT/DONE.txt\\\");f.open(\\\"w\\\");f.write(\\\"OUTER ERROR: \\\"+e.toString());f.close();}\"" &
OSA_PID=$!
# restore runs on EVERY exit (a timed-out run once leaked quiet-mode + the stashed dev
# shell into live state, found 2026-09-02) — armed here, after snapshot + stash happened
restore_state() {
  for pd in "$PANELS_GLOB"/*/Scripts/ScriptUI\ Panels; do
    [ -f "$pd/minColor2 dev.jsx.suite-stash" ] && mv "$pd/minColor2 dev.jsx.suite-stash" "$pd/minColor2 dev.jsx" || true
  done
  # restore settings — but keep the handshake the AEGP just wrote: aegp-api.json is
  # last-launch info by design, and restoring a stale copy would make it lie
  [ -f "$SETTINGS/aegp-api.json" ] && cp "$SETTINGS/aegp-api.json" "$OUT/.aegp-api.json" || true
  if [ -d "$SETTINGS_BAK" ]; then rm -rf "$SETTINGS"; cp -R "$SETTINGS_BAK" "$SETTINGS"; fi
  [ -f "$OUT/.aegp-api.json" ] && mkdir -p "$SETTINGS" && cp "$OUT/.aegp-api.json" "$SETTINGS/aegp-api.json" || true
}
trap restore_state EXIT

for i in $(seq 1 600); do [ -f "$OUT/DONE.txt" ] && break; sleep 2; done
[ -f "$OUT/DONE.txt" ] || { echo "TIMEOUT: no DONE marker"; kill $OSA_PID 2>/dev/null || true; exit 1; }
wait $OSA_PID 2>/dev/null || true
ae_quit

grep -q "^done" "$OUT/DONE.txt" || { echo "driver failed:"; cat "$OUT/DONE.txt"; exit 1; }
rm -rf "$OUT"/scratch-* "$WRAP"

# M1 equivalence scenarios (13-19) RETIRED at the M3 contract flip: the 0.9.2 panel is no
# longer the reference — scenarios 01-12 drive the native commands directly, so this loop
# matching nothing is deliberate (kept for any future *-equiv-* self-verifying scenario).
for eq in "$OUT"/*-equiv-*.txt; do
  [ -e "$eq" ] || continue
  grep -q "EQUIVALENT: YES" "$eq" || { echo "EQUIVALENCE RED: $(basename "$eq")"; grep -v "^$" "$eq" | tail -20; exit 1; }
done

if [ "$MODE" = "--record" ]; then
  mkdir -p "$BASE/golden"
  rm -f "$BASE/golden"/*.txt
  cp "$OUT"/*.txt "$BASE/golden/"
  rm -f "$BASE/golden/DONE.txt" "$BASE/golden/"*-equiv-*.txt   # equiv scenarios are self-verifying, never goldened
  echo "recorded $(ls "$BASE/golden" | wc -l | tr -d ' ') goldens"
else
  FAIL=0
  for g in "$BASE/golden"/*.txt; do
    n="$(basename "$g")"
    if ! diff -q "$g" "$OUT/$n" >/dev/null 2>&1; then
      echo "DIFF: $n"; diff "$g" "$OUT/$n" | head -20 || true; FAIL=1
    fi
  done
  [ "$FAIL" = "0" ] && echo "BASELINE GREEN ($(ls "$BASE/golden" | wc -l | tr -d ' ') scenarios)" || { echo "BASELINE RED"; exit 1; }
fi
