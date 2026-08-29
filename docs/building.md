# Building and releasing

## Layout

- `src/` — panel and libraries (`minColor.jsxinc`, `AEPPatch.jsxinc`)
- `config/` — `generate.py`, vendored master, generated `dist/` (committed, append-only)
- `plugin/` — effect sources, CMake build, `tools/probe-engine`, `prebuilt/windows/`
- `build/build.py` — the distributable
- `packaging/macos/`, `packaging/windows/` — installers

## Build

```
python3 config/generate.py                              # configs -> config/dist/ (self-validating)
plugin/external/build.sh                                # static OCIO 2.5.2, once (build.bat on Windows)
cmake -S plugin -B plugin/build && cmake --build plugin/build
python3 build/build.py                                  # dist-panel/: panel, payload, both engines, zip
```

`plugin/scripts/dev-install.sh|.bat` installs the built plug-in (copy, never symlink);
`dev-install-panel.bat` builds and installs the panel per user. Restart After Effects after a
plug-in change.

## Versions

Panel: `var VERSION` in `src/minColor.jsxinc`. Engine: `plugin/src/MinColorCST.h`,
`Info.plist.in` and `MinColorCST_PiPL.r` together (a mismatch triggers After Effects' version
dialog on Windows). The Windows `.aex` is built with VS2022 and committed at
`plugin/prebuilt/windows/` with its `version.txt`.

## Signing (macOS)

The build re-signs the bundle after the PiPL is copied in (ad-hoc by default; pass
`-DMINC_CODESIGN_IDENTITY="Developer ID Application: …"` for a hardened, timestamped signature).
Downloaded bundles must also be notarized:

```
NOTARY_PROFILE=<profile> plugin/scripts/notarize.sh     # sign, notarize, staple
```

`<profile>` is a notarytool keychain profile (`xcrun notarytool store-credentials`). Run this
before `build/build.py`.

## Installers

- macOS: `NOTARY_PROFILE=<profile> packaging/macos/build-pkg.sh` → `dist-panel/minColor-<ver>.pkg`
  (signed with the Developer ID Installer identity and notarized when present). Log:
  `/var/log/minColor-install.log`. Removal: `packaging/macos/uninstall.command`.
- Windows: `powershell -ExecutionPolicy Bypass -File packaging\windows\build.ps1` (WiX v5) →
  `dist-panel\minColor-<ver>.msi`. Silent: `msiexec /i minColor-<ver>.msi /qn`.

## Release

1. `python3 config/generate.py`; commit new configs (never delete old ones).
2. Bump versions; rebuild the engine on both platforms; commit the Windows prebuilt.
3. macOS: `notarize.sh` → `build.py` → `build-pkg.sh`. Windows: `build.py` → `build.ps1`.
4. Tag `vX.Y.Z`; upload the pkg, the msi and the zip.

## Checking a build

- `plugin/tools/probe-engine <config> <working> to|from <space> r g b` — the engine without After
  Effects; compare with PyOpenColorIO.
- `plugin/tools/sdat-scan.py <project.aep>` — every minColor effect's stored state.
- `/tmp/minColorCST_authority.log` (`%TEMP%` on Windows) — the plug-in's log.
