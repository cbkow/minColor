# Building and releasing

## Build

```
python3 config/generate.py                              # configs -> config/dist/
plugin/external/build.sh                                # static OCIO, once (build.bat on Windows)
cmake -S plugin -B plugin/build && cmake --build plugin/build   # BOTH bundles (effects + AEGP)
python3 build/build.py                                  # dist-panel/: shell, configs, plugins, zip
```

To test a build locally, `plugin/scripts/dev-install-both.sh` copies the effects bundle into
MediaCore **and** the ceremonies AEGP into the After Effects app's Plug-ins folder (the second
copy asks for an administrator). Quit After Effects first; restart it after.

The Windows plugin (.aex, five effects including legacy `MINC CST`) is built with Visual Studio
2022 and committed at `plugin/prebuilt/windows/` so a Mac can build the full release zip. The
0.9.x panel keeps shipping for Windows (from `private/attic/`, inlined by `build.py`) until the
2.0 engine lands there.

## Versions

One number: `MINC_VERSION` in `plugin/CMakeLists.txt`. The build stamps it into both bundles,
the handshake file, provenance and `version.txt`; `build.py` and `build-pkg.sh` read it from
there. (`MINC_PIPL_VERSION_HEX` in `plugin/src/core/MincIds.h` is the PiPL-format version —
bump it only when effect parameters change shape.)

## Signing (macOS)

Downloaded plugins have to be signed and notarized or macOS calls them damaged. After building:

```
NOTARY_PROFILE=<profile> plugin/scripts/notarize.sh
```

`<profile>` is a notarytool keychain profile you've set up once with
`xcrun notarytool store-credentials`. Do this before `build/build.py`.

## Installers

- **macOS:** `NOTARY_PROFILE=<profile> packaging/macos/build-pkg.sh` builds
  `dist-panel/minColor-<ver>.pkg`, signed and notarized if a Developer ID Installer certificate
  is in the keychain. It installs the effects into MediaCore, the AEGP into every
  After Effects ≥ 2025 in /Applications, and the panel for the current user.
  `packaging/macos/uninstall.command` removes everything.
- **Windows:** `powershell -ExecutionPolicy Bypass -File packaging\windows\build.ps1` builds
  `dist-panel\minColor-<ver>.msi` with WiX. `msiexec /i minColor-<ver>.msi /qn` installs silently.

## Release

1. Regenerate configs and commit them (never delete old ones).
2. Bump `MINC_VERSION`, rebuild the plugin on both platforms, commit the Windows prebuilt.
3. Mac: notarize → `build.py` → `build-pkg.sh`. Windows: `build.py` → `build.ps1`.
4. Tag `vX.Y.Z` and upload the pkg, the msi and the zip.

## Checking a build

- `tests/baseline/run.sh --check` runs the 26-scenario suite against the installed pair
  (quit After Effects first; `--record` re-records goldens — only after a reviewed change).
- `plugin/tools/probe-engine <config> <working> to|from <space> r g b` runs the plugin's engine
  outside After Effects.
- `plugin/tools/sdat-scan.py <project.aep>` lists every minColor effect stored in a project.
- The effects log to `/tmp/minColorCST_authority.log`, the ceremonies to
  `/tmp/minColorAEGP.log` (`%TEMP%` on Windows).
