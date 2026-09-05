# Building and releasing

## Build

```
python3 config/generate.py                              # configs -> config/dist/ (build input)
plugin/external/build.sh                                # static OCIO, once (build.bat on Windows)
cmake -S plugin -B plugin/build && cmake --build plugin/build   # BOTH bundles (effect + AEGP)
python3 build/build.py                                  # dist-panel/: shell + zip
```

The CMake build runs two Python codegens (so **Python 3 must be on PATH**): one bakes the configs + LUTs into the effect, the other bakes the metadata (`presets.json`, `extension-defaults.json`, `render-presets.json`, config text) into the AEGP. Nothing else is installed to disk — the binaries are the package.

To test a build locally:
- **macOS:** `plugin/scripts/dev-install-both.sh` copies the effect into MediaCore **and** the ceremonies AEGP into the After Effects app's Plug-ins folder (the second copy asks for an administrator), and installs the shell for the current user.
- **Windows:** `plugin\scripts\dev-install.bat` (effect + AEGP, self-elevates) then `plugin\scripts\dev-install-panel.bat` (the shell, per-user). The panel script also purges any old 0.9.x panel so AE loads only the 2.0 shell.

Quit After Effects first; restart it after.

The Windows plugins (`minColorCST.aex` + `minColorAEGP.aex`, five effects including legacy `MINC CST`) are built with Visual Studio 2022 and committed at `plugin/prebuilt/windows/` so a Mac can build the full release zip. See `plugin/WINDOWS.md` for the Windows build in full.

## Versions

One number: `MINC_VERSION` in `plugin/CMakeLists.txt`. The build stamps it into both bundles, the handshake file, provenance and `version.txt`; `build.py` and `build-pkg.sh` read it from there. (`MINC_PIPL_VERSION_HEX` in `plugin/src/core/MincIds.h` is the PiPL-format version — bump it only when effect parameters change shape.)

## Signing (macOS)

Downloaded plugins have to be signed and notarized or macOS calls them damaged. After building:

```
NOTARY_PROFILE=<profile> plugin/scripts/notarize.sh
```

`<profile>` is a notarytool keychain profile you've set up once with `xcrun notarytool store-credentials`. It signs + notarizes + staples **both** bundles. Do this before `build/build.py` and `build-pkg.sh`.

## Installers

The installer payload is the two binaries + the shell — no config store, no settings seed (the AEGP seeds settings from its embed on first launch).

- **macOS:** `NOTARY_PROFILE=<profile> packaging/macos/build-pkg.sh` builds `dist-panel/minColor-<ver>.pkg`, signed and notarized if a Developer ID Installer certificate is in the keychain. It installs the effect into MediaCore, the AEGP into every After Effects ≥ 2025 in /Applications, and the panel for the current user. `packaging/macos/uninstall.command` removes everything.
- **Windows:** `powershell -ExecutionPolicy Bypass -File packaging\windows\build.ps1` builds `dist-panel\minColor-<ver>.msi` with WiX. `msiexec /i minColor-<ver>.msi /qn` installs silently.

## Release

1. Regenerate configs (`config/generate.py`) and commit `config/dist` — the build input the codegens embed.
2. Bump `MINC_VERSION`, rebuild both bundles on both platforms, commit the Windows prebuilt.
3. Mac: notarize → `build.py` → `build-pkg.sh`. Windows: refresh prebuilt → `build.py` → `build.ps1`.
4. Tag `vX.Y.Z` and upload the pkg, the msi and the zip.

## Checking a build

- `tests/baseline/run.sh --check` runs the 20-scenario suite against the installed pair (quit After Effects first; `--record` re-records goldens — only after a reviewed change). It's store-independent: it passes with or without a config store on disk.
- `plugin/tools/probe-engine <config> <working> to|from <space> r g b` runs the plugin's engine outside After Effects.
- `plugin/tools/sdat-scan.py <project.aep>` lists every minColor effect stored in a project.
- The effects log to `/tmp/minColorCST_authority.log`, the ceremonies to `/tmp/minColorAEGP.log` (`%TEMP%` on Windows).
