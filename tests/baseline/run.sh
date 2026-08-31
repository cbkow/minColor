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

ae_running() { osascript -e "tell application \"System Events\" to (name of processes) contains \"After Effects $AE_VER\"" ; }
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
for i in $(seq 1 600); do [ -f "$OUT/DONE.txt" ] && break; sleep 2; done
[ -f "$OUT/DONE.txt" ] || { echo "TIMEOUT: no DONE marker"; kill $OSA_PID 2>/dev/null || true; exit 1; }
wait $OSA_PID 2>/dev/null || true
ae_quit

# restore settings
if [ -d "$SETTINGS_BAK" ]; then rm -rf "$SETTINGS"; cp -R "$SETTINGS_BAK" "$SETTINGS"; fi

grep -q "^done" "$OUT/DONE.txt" || { echo "driver failed:"; cat "$OUT/DONE.txt"; exit 1; }
rm -rf "$OUT"/scratch-* "$WRAP"

if [ "$MODE" = "--record" ]; then
  mkdir -p "$BASE/golden"
  rm -f "$BASE/golden"/*.txt
  cp "$OUT"/*.txt "$BASE/golden/"
  rm -f "$BASE/golden/DONE.txt"
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
