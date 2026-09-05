# minColor plugin — Windows build (lean-v3)

STATUS: the **effect** (`minColorCST.aex`) builds on Windows and now **embeds its OCIO configs +
LUTs** in the binary (zero-filesystem render). The **AEGP** (ceremonies bundle) is still built
ONLY in the CMake `if(APPLE)` branch — **porting it is the open Windows milestone** (bottom of
this doc). Until it lands, Windows is effect-only and the lean-v3 workflow/panel don't run there.

## 0. Sync first — this is a different codebase than the last Windows build

Pull the lean-v3 rebuild (merged to `main`):

```
git checkout main
git pull
```

What changed since Windows last built (all on `main` now):
- The effect is **self-contained**: colour lives in a saved param (native **Space popup** in
  Effect Controls — the old clickable "badge" is gone), read at render.
- The effect **embeds every preset config + its LUTs** (zlib) and renders from the binary — no
  filesystem, no dead config pins. AE pins a lean per-preset "interface" config; the effect
  ignores it and uses its own embedded config.
- The ceremonies (Migrate, Interpret, Utility, Apply Look, Render Preset, Strip, Doctor/Repair)
  and the handshake the panel gates on live in the **AEGP**. Set Up / Archive / Package / Adopt
  were retired; Migrate is the single managed-project entry.

## 1. Prereqs

- Visual Studio 2022 (C++ workload), CMake 3.24+, git — "x64 Native Tools Command Prompt for VS".
- **Python 3 on PATH — NEW.** The build runs `plugin/cmake/gen_embedded_configs.py` to bake the
  configs + LUTs into the effect. `find_program(... python3 python REQUIRED)` — configure FAILS
  without it.

## 2. Build the effect

1. SDK: unzip the Windows AE SDK so `plugin\sdk\<x>\Examples\Headers\AE_Effect.h` exists
   (the SDK archive doesn't travel with the repo — copy from `private/sdk/` on the mac).
2. Static OCIO 2.5.2: `plugin\external\build.bat` → `external\install` (built /MD;
   `CMAKE_MSVC_RUNTIME_LIBRARY MultiThreadedDLL` matches AE). This also builds **zlib** into
   `external\build\ext\dist` — the embed decompressor links it (`libz`/`zlib*.lib` via the
   `OCIO_EXT_LIBS` glob) and the effect CMake adds `ext/dist/include` for `zlib.h`.
3. Build:
   ```
   cmake -S plugin -B plugin\build
   cmake --build plugin\build --config Release      -> minColorCST.aex
   ```
   The embed codegen runs automatically (`add_custom_command` → `gen/MincEmbeddedConfigs.cpp`,
   ~18 MB generated source; regenerates when `config\dist` changes). PiPL as before
   (`MinColorCST_PiPL.r` → `cl /EP` → PiPLtool → `.rc`, via `cmake/BuildPiPL.cmake`); the five
   effects build on Windows (legacy `MINC CST` is `#ifdef AE_OS_WIN`).
4. Install: copy `minColorCST.aex` to
   `C:\Program Files\Adobe\Common\Plug-ins\7.0\MediaCore\minColor\`.
   The `configs` folder beside it is **no longer required for rendering** (embedded) — keep it
   only for the AEGP's disk reads once that lands.
5. Verify: restart AE → apply a minColor effect → its **Space popup** populates and a render
   transforms. Log: `%TEMP%\minColorCST_authority.log`. AE-free engine check:
   `plugin\tools\build-probe.bat` (verify vs PyOpenColorIO).

## 3. The AEGP on Windows — shared C++ is now portable; toolchain bits remain

Everything workflow-side (Migrate/Interpret/Utility/Doctor, `MincWriteMenus`, the
`settings/aegp-api.json` handshake the panel requires) is in `minColorAEGP`, which the CMake
still builds only under `if(APPLE)`. **The shared C++ was made Windows-ready** (this is done, on
`main`):
- POSIX file ops (`mkdir`/`opendir`-`readdir`/`unlink`/`localtime_r`, `<dirent.h>`/`<unistd.h>`)
  in the ceremony code are now `std::filesystem` via `src/ceremony/MincFs.h` (`mfs::mkdirs`,
  `copyTree`, `removeFile`, `listFiles`, `localTime` — the last is the only `#ifdef _WIN32`,
  `localtime_s` vs `localtime_r`). C++17, compiles on both. `stat()` (existence checks) stays —
  MSVC provides it with `_CRT_SECURE_NO_WARNINGS` (already defined for the effect target).
- The preset picker is split: `MincPicker.mm` (mac, NSAlert) and **`MincPicker.cpp` (Windows,
  already written)** — the portable shell-args/quiet-answers tiers, no dialog (panel-driven use
  needs none; a menu-invoked command with no panel returns false).

So the Windows session only needs the **toolchain wiring**:

1. **CMake `elseif(WIN32)` — add the `minColorAEGP` target** mirroring the mac one: sources =
   `src/aegp/AegpMain.cpp` + `src/ceremony/*.cpp` (**use `MincPicker.cpp`, NOT `MincPicker.mm`**)
   + `${MINC_CORE_SOURCES}`; include dirs `${MINC_COMMON_INCLUDES}`; link the OCIO static libs
   (same as the effect) — **no `-framework AppKit`**. Compile-def `MINC_LOG_BASENAME="minColorAEGP"`.
   Build as a DLL renamed `.aex`; the AEGP entry (`MincAegpEntry`) is the one `__declspec(dllexport)`.
2. **Windows AEGP PiPL** — `src/aegp/MinColorAEGP_PiPL.r` through the same `.r → cl /EP → PiPLtool
   → .rc` path as the effect (Kind `AEGP`, entry `MincAegpEntry`), linked into the `.aex`.
3. **Install:** AEGP `.aex` → each `Adobe After Effects <ver>\Plug-ins\`; shell panel
   `src/minColor Shell.jsx` → `%APPDATA%\Adobe\After Effects\<ver>\Scripts\ScriptUI Panels\minColor.jsx`;
   ship `config\dist` (configs + LUT trees) to
   `C:\Program Files\Adobe\Common\Plug-ins\7.0\MediaCore\minColor\configs\` for the AEGP's disk
   reads (the effect is embedded and doesn't need them).
4. **Verify:** restart AE → the minColor menu commands exist (About, Doctor, Migrate, Interpret…);
   the panel loads past its handshake gate; Migrate + Interpret a project.

Portability rules unchanged: platform code behind `#ifdef AE_OS_WIN` (defined by the Windows CMake
branch alongside `MSWindows`); never fork shared logic.

## 4. The panel/installer — Windows is on the 2.0 shell now (the 0.9.2 lane is retired)

There is **one panel**: `src/minColor Shell.jsx` (mac and Windows). The old `minColor Panel.jsx`
(from `private/attic`) and its `minColor-data` payload are gone from the build. `build/build.py`
and `packaging/windows/build.ps1` are already updated (they ship `dist-panel/minColor.jsx` for
Windows and stage `minColorAEGP.aex` for the installer). The shell **gates on the AEGP handshake**
(`settings/aegp-api.json`), so it does nothing until the Windows AEGP (§3) is installed — that's
why a lean-v3 Windows install needs the AEGP, not just the effect.

**`packaging/windows/minColor.wxs` still needs two edits (WiX, build+test on Windows):**
1. **Drop `minColor-data`:** remove the `AE2025DATA`/`AE2026DATA` directories and the
   `<Files Include="…\minColor-data\**" …>` lines from `Panel2025`/`Panel2026`. The `minColor.jsx`
   component stays (now the 2.0 shell).
2. **Add the AEGP component:** install the staged `minColorAEGP.aex` into each AE's
   `…\Adobe After Effects <ver>\Support Files\Plug-ins\` (a new per-AE `Plug-ins` Directory +
   Component, feature-gated like the panels). Without it the panel installs but stays gated.

`build.ps1` warns if `plugin\prebuilt\windows\minColorAEGP.aex` is missing, so build the WIN32 AEGP
target (§3) and commit it to `plugin\prebuilt\windows` before cutting the .msi.
