# minColor plugin — Windows build (lean-v3)

STATUS: **both bundles build on Windows and both are self-contained.** The **effect**
(`minColorCST.aex`) embeds its OCIO configs + LUTs; the **AEGP** (`minColorAEGP.aex`, ceremonies +
handshake) embeds `presets.json` + `extension-defaults.json` + `render-presets.json` + the config
text, and **seeds `C:\ProgramData\minColor\settings` from those on first launch**. The installer
ships **only the two binaries + the shell** — no config store, no settings seed. Verified store-less
on the mac (baseline suite green with no config store on disk); Windows should behave the same.

If you last built before this, your panel "couldn't see the OCIO profiles" because the AEGP read
`presets.json` off disk and a bundles-only install had no store. That's fixed: the AEGP now carries
the metadata and seeds what the shell reads. **Rebuild the AEGP and the symptom goes away.**

## 0. Sync first

```
git checkout main
git pull                     # need cb28783 + 623ca22 (AEGP embed + self-seed + slim installers)
```

## 1. Prereqs

- Visual Studio 2022 (C++ workload), CMake 3.24+, git — "x64 Native Tools Command Prompt for VS".
- **Python 3 on PATH.** The build runs TWO codegens (both `find_program(... python3 python REQUIRED)`
  — configure FAILS without Python):
  - `cmake/gen_embedded_configs.py` → bakes configs + LUTs into the **effect** (~18 MB source).
  - `cmake/gen_embedded_meta.py` → bakes `presets.json` + `extension-defaults.json` +
    `render-presets.json` + config text into the **AEGP** (~0.9 MB, raw — no zlib needed).
- Windows AE SDK unzipped so `plugin\sdk\<x>\Examples\Headers\AE_Effect.h` exists (copy from
  `private/sdk/` on the mac; the SDK doesn't travel with the repo).
- Static OCIO 2.5.2: `plugin\external\build.bat` → `external\install` (also builds zlib into
  `external\build\ext\dist`, which the effect's embed decompressor links).

## 2. Build both bundles

```
cmake -S plugin -B plugin\build
cmake --build plugin\build --config Release
```

Produces `minColorCST.aex` (effect) and `minColorAEGP.aex` (ceremonies). Both codegens run
automatically and regenerate when `config\dist` / `config\*.json` change. The AEGP target is the
`elseif(WIN32)` branch — it links the OCIO static libs (same as the effect), uses `MincPicker.cpp`
(not the `.mm`), and builds the embedded-meta cpp; its PiPL goes through the same `.r → cl /EP →
PiPLtool → .rc` path as the effect.

**Nothing else in the C++ changed this session** — the AEGP was already cross-platform (WIN32 target
+ `std::filesystem` via `MincFs.h` + platform-aware paths). This session only added the embedded
metadata + self-seed, wired into the existing WIN32 target. So this is a plain rebuild.

## 3. Refresh the prebuilt AEGP (what the installer ships)

`build.ps1` installs the AEGP from `plugin\prebuilt\windows` (it throws if it's missing). After the
rebuild, refresh it and commit:

```
copy /Y plugin\build\Release\minColorAEGP.aex plugin\prebuilt\windows\minColorAEGP.aex
copy /Y plugin\build\Release\minColorCST.aex  plugin\prebuilt\windows\minColorCST.aex
copy /Y plugin\build\Release\version.txt      plugin\prebuilt\windows\version.txt
git add plugin\prebuilt\windows & git commit -m "Windows: refresh prebuilt (AEGP embeds metadata)"
```

(The effect binary didn't change logically this session, but refresh it too so the prebuilt set is
one coherent build. `version.txt` is the DefaultVersion the MSI compares against.)

## 4. Build the installer — ships ONLY the two binaries + the shell

`packaging\windows\build.ps1` and `packaging\windows\minColor.wxs` are **already updated** on `main`:
no config store (`ConfigsCentral`/`ConfigsShared` gone), no settings seed — the effect embeds its
configs+LUTs and the AEGP seeds `ProgramData\minColor\settings` on launch. Just build it:

```
python build\build.py                                          # stages dist-panel\minColor.jsx (the shell)
powershell -ExecutionPolicy Bypass -File packaging\windows\build.ps1
```

→ `dist-panel\minColor-<ver>.msi`. It lays down `minColorCST.aex` (MediaCore), and per installed AE
≥2025 the `minColorAEGP.aex` (Support Files\Plug-ins) + `minColor.jsx` (ScriptUI Panels). That's the
whole payload.

## 5. Verify (store-less is the point)

Restart AE, then:
- minColor menu commands exist (About, Doctor, Migrate, Interpret…).
- The **panel loads past its handshake gate** and its **dropdowns populate** — this is the fix; it
  works with no `configs` folder on disk anywhere.
- Migrate a saved project → OCIO on, a `_minColor\config-<preset>-interface.ocio` written beside the
  `.aep`, effect renders (its **Space popup** populates and transforms).
- Logs: `%TEMP%\minColorAEGP.log` (should say `presets: <embedded>/presets.json` when no store is on
  disk) and `%TEMP%\minColorCST_authority.log`.

To prove store-less explicitly: there should be **no**
`C:\Program Files\Adobe\Common\Plug-ins\7.0\MediaCore\minColor\configs\` and no
`C:\ProgramData\minColor\configs\` after a clean MSI install — only
`C:\ProgramData\minColor\settings\` (seeded by the AEGP at launch).

Portability rule unchanged: platform code behind `#ifdef AE_OS_WIN` / `_WIN32`; never fork shared logic.
