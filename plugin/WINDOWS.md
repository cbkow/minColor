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

## 3. Open milestone — the AEGP on Windows

Everything workflow-side (Migrate/Interpret/Utility/Doctor, `MincWriteMenus`, the
`settings/aegp-api.json` handshake the panel requires) is in `minColorAEGP`, built only under
`if(APPLE)` today. To ship lean-v3 on Windows:

- Add a `WIN32` branch that builds `minColorAEGP` as a Kind-AEGP `.aex`/DLL, mirroring the mac
  target (same `MINC_CORE_SOURCES` + `src/ceremony/*` + `src/aegp/AegpMain.cpp`), with a Windows
  AEGP PiPL.
- The mac target links `-framework AppKit` for the NSAlert preset picker (`MincPicker.mm`).
  Replace/stub it: `MincPickPreset` reads `shell-args.json`/`quiet-answers.json` FIRST, so
  panel-driven use needs no native picker — only menu-invoked commands do. `#ifdef AE_OS_WIN`
  the Objective-C++ picker and provide a Win32 dialog (or a no-op that relies on the panel).
- Install: AEGP → each `Adobe After Effects <ver>\Plug-ins\`; shell panel `src/minColor Shell.jsx`
  → `%APPDATA%\Adobe\After Effects\<ver>\Scripts\ScriptUI Panels\minColor.jsx`; ship `config\dist`
  to the store for the AEGP's disk reads.

Portability rules unchanged: platform code behind `#ifdef AE_OS_WIN` (defined by the Windows
CMake branch alongside `MSWindows`); never fork shared logic.
