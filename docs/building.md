# Building and releasing

## Build

```
python3 config/generate.py                              # configs -> config/dist/
plugin/external/build.sh                                # static OCIO, once (build.bat on Windows)
cmake -S plugin -B plugin/build && cmake --build plugin/build
python3 build/build.py                                  # dist-panel/: panel, configs, both plugins, zip
```

To test a build locally, `plugin/scripts/dev-install.sh` (or `.bat`) copies the plugin into
MediaCore and `dev-install-panel.bat` installs the panel. Restart After Effects after a plugin
change.

The Windows plugin is built with Visual Studio 2022 and committed at `plugin/prebuilt/windows/`
so a Mac can build the full release zip.

## Versions

- Panel: `var VERSION` in `src/minColor.jsxinc`.
- Plugin: `plugin/src/MinColorCST.h`, `Info.plist.in` and `MinColorCST_PiPL.r` — change all three
  together or After Effects on Windows complains about a version mismatch.

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
  is in the keychain. `packaging/macos/uninstall.command` removes everything.
- **Windows:** `powershell -ExecutionPolicy Bypass -File packaging\windows\build.ps1` builds
  `dist-panel\minColor-<ver>.msi` with WiX. `msiexec /i minColor-<ver>.msi /qn` installs silently.

## Release

1. Regenerate configs and commit them (never delete old ones).
2. Bump versions, rebuild the plugin on both platforms, commit the Windows prebuilt.
3. Mac: notarize → `build.py` → `build-pkg.sh`. Windows: `build.py` → `build.ps1`.
4. Tag `vX.Y.Z` and upload the pkg, the msi and the zip.

## Checking a build

- `plugin/tools/probe-engine <config> <working> to|from <space> r g b` runs the plugin's engine
  outside After Effects.
- `plugin/tools/sdat-scan.py <project.aep>` lists every minColor effect stored in a project.
- The plugin logs to `/tmp/minColorCST_authority.log` (`%TEMP%` on Windows).
