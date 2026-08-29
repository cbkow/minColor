# minColor — building and releasing

## Layout

- `src/` — panel (`minColor Panel.jsx`) and libraries (`minColor.jsxinc` core,
  `AEPPatch.jsxinc` RIFX reader/patcher)
- `config/` — `generate.py`, vendored master, patches, generated `dist/` (committed, append-only)
- `plugin/` — effect/AEGP sources, CMake build, PiPL, `tools/probe-engine`, `prebuilt/windows/`
- `build/build.py` — the distributable (panel + payload + both engines + zip)
- `packaging/macos/`, `packaging/windows/` — installers

## Build

```
python3 config/generate.py          # config family -> config/dist/ (validates every config; SDR checks run in a subprocess)
plugin/external/build.sh            # static OCIO 2.5.2 (build.bat on Windows), once
cmake -S plugin -B plugin/build && cmake --build plugin/build
python3 build/build.py              # dist-panel/: minColor.jsx, minColor-data/, plugin-macOS/, plugin-windows/, zip
```

Dev installs: `plugin/scripts/dev-install.sh|.bat` copies the built plug-in (Windows: plus the
config store) into MediaCore — never symlink, After Effects does not register effects from a
symlinked bundle; `plugin/scripts/dev-install-panel.bat` builds and installs the panel per user
(macOS: copy `dist-panel/minColor.jsx` + `minColor-data/` into
`~/Library/Preferences/Adobe/After Effects/<ver>/Scripts/ScriptUI Panels/`). After Effects must
restart to load a new plug-in.

## Versions

- Panel: `var VERSION` in `src/minColor.jsxinc` (bundle zip and installers take their version
  from it).
- Engine: bump all three together — `plugin/src/MinColorCST.h`, `plugin/src/Info.plist.in`,
  `plugin/src/MinColorCST_PiPL.r` (`AE_Effect_Version` = `PF_VERSION(major, minor, bug, DEVELOP,
  build)`); a mismatch raises After Effects' version-mismatch dialog on Windows.
- The Windows `.aex` is built on a Windows box (VS2022) and committed at `plugin/prebuilt/windows/`
  with its `version.txt`; `build.py` bundles it and reads its version.

## Signing and notarization (macOS)

The CMake post-build re-signs the bundle after the PiPL resource is copied in (ad-hoc by default;
`-DMINC_CODESIGN_IDENTITY="Developer ID Application: …"` for a hardened-runtime, timestamped
signature). A downloaded bundle also needs notarization or Gatekeeper reports it as damaged:

```
NOTARY_PROFILE=<profile> plugin/scripts/notarize.sh    # sign, notarytool submit --wait, staple, validate
```

`<profile>` is a notarytool keychain profile created once with
`xcrun notarytool store-credentials <profile> --apple-id … --team-id …`. `spctl --type execute`
cannot judge plug-in bundles; `xcrun stapler validate` and `codesign --verify --deep --strict`
are the proof. Run this before `build/build.py` so the zip and the pkg carry the notarized bundle.

## Installers

- **macOS** — `NOTARY_PROFILE=<profile> packaging/macos/build-pkg.sh` → `dist-panel/minColor-<ver>.pkg`.
  Signed with the "Developer ID Installer" identity and notarized when present, otherwise an
  unsigned pkg for local testing. Payload: plug-in + config store into MediaCore, panel into every
  After Effects ≥ 2025 for the console user, settings seeded under `/Users/Shared/minColor/settings`
  (never overwritten). Refuses to run while After Effects is open. Log:
  `/var/log/minColor-install.log`. Removal: `packaging/macos/uninstall.command`.
- **Windows** — `powershell -ExecutionPolicy Bypass -File packaging\windows\build.ps1` (WiX v5)
  → `dist-panel\minColor-<ver>.msi`, per-machine: engine + configs into MediaCore, panel into each
  installed After Effects ≥ 2025 (app-level `Support Files\Scripts\ScriptUI Panels`), settings
  under `ProgramData\minColor` (Permanent, NeverOverwrite). Unsigned. Silent:
  `msiexec /i minColor-<ver>.msi /qn`. A dev box with the per-user panel from
  `dev-install-panel.bat` should remove that copy to avoid two entries in the Window menu.
- Both merge the config store append-only.

## Release checklist

1. `python3 config/generate.py` — commit the new hashed configs (never delete old ones).
2. Bump versions (panel; engine if `plugin/src` changed) and rebuild the engine on both platforms;
   commit the Windows prebuilt.
3. macOS: `notarize.sh` → `build/build.py` → `build-pkg.sh`. Windows: `build\build.py` → `build.ps1`.
4. Tag `vX.Y.Z`; upload the pkg, the msi and the zip.

## Verification tools

- `plugin/tools/probe-engine <config> <working> to|from|look <space> r g b …` — the plug-in's
  engine without After Effects, 8 decimal places; compare with PyOpenColorIO.
- `plugin/tools/sdat-scan.py <project.aep>` — every minColor effect's stored state (direction,
  space, instance id, passport) from a saved project.
- `/tmp/minColorCST_authority.log` (`%TEMP%\minColorCST_authority.log`) — the plug-in's authority
  snapshots and sync results.
- The generator's own validation: ociocheck, PyOpenColorIO load, file-rule resolution, view
  builds, and for the SDR config bridge exactness, master fidelity, display purity and
  nested-reference asserts.
