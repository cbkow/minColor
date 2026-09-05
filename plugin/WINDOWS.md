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

## Updating to 2.0.2 (do this now)

Since the last Windows prebuilt (**2.0.1 @ee04c3b**), BOTH the **plugin** and the **script** changed —
so rebuild the bundles *and* refresh the shell. What landed on `main`:
- **`f35e681` — Repair goes live + all `.aep` backups removed (AEGP change).** Repair no longer
  RIFX-patches the project file and reopens — it does a **live re-pin** via the same
  `AEGP_ExecuteScript` bridge Migrate uses. And Migrate + Repair no longer force-save or write a
  `_minColor\backups` copy: **nothing edits or backs up the `.aep` anymore** (both are live, AE-undo
  covered until the user saves). This is in the **AEGP**, so it must be rebuilt.
- **`src/minColor Shell.jsx` (the SCRIPT) changed with it.** The Migrate dialog now reads *"Live —
  undo before you save to revert"* (was "Backs up first") and the migrate summary drops the backups
  count. The MSI ships the shell, so it must be refreshed too.
- **Version → 2.0.2** — rebuild both bundles so `version.txt` reads 2.0.2 (the MSI's DefaultVersion).

**Update checklist (details in the numbered sections below):**
1. `git checkout main && git pull`.
2. Rebuild **both** bundles: `cmake --build plugin\build --config Release` (§2). Confirm
   `plugin\build\Release\version.txt` reads **2.0.2**.
3. Refresh the prebuilt + commit (§3): copy `minColorCST.aex`, `minColorAEGP.aex`, `version.txt`
   from `plugin\build\Release` to `plugin\prebuilt\windows\`.
4. Rebuild the MSI (§4): `python build\build.py` (re-inlines the updated shell into
   `dist-panel\minColor.jsx`, which the MSI ships) then `build.ps1`.
5. Verify (§5): version **2.0.2**; **Repair** re-points a broken pin live (no reopen); and neither
   Migrate nor Repair creates a `_minColor\backups` folder.

For a quick dev loop instead of the MSI: `dev-install.bat` (both bundles) + `dev-install-panel.bat`
(the updated shell) — §2b.

## 0. Sync first

```
git checkout main
git pull                     # need through the 2.0.2 bump (Repair live + backups removed; also 2.0.1 interpret fix + panel theme)
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

## 2b. Dev iteration without the MSI (counterpart of dev-install-both.sh)

For a quick build→test loop, use the batch installers instead of cutting an MSI:

```
plugin\scripts\dev-install.bat          rem effect -> MediaCore, AEGP -> each AE's Plug-ins (self-elevates)
plugin\scripts\dev-install-panel.bat    rem 2.0 shell -> per-user ScriptUI Panels (no elevation)
```

Both were 0.9.2-era and are fixed for lean-v3: `dev-install.bat` now also installs the AEGP and ships
NO config store; `dev-install-panel.bat` installs the 2.0 shell and **purges the old 0.9.x panel**
(`minColor-data` + `minColor Panel.jsx`) so AE stops loading it. If AE still shows the old panel, an
old **MSI** left a per-machine copy — uninstall it, or delete
`…\Adobe After Effects <ver>\Support Files\Scripts\ScriptUI Panels\minColor*.jsx`.

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
