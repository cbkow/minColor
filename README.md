# minColor

An OCIO colour-management pipeline for After Effects — no compiled plugin, just AE's
native OCIO mode, its native Color Space Transform effect, and a dockable ScriptUI panel.

## Model

- **Working-space presets**: ACEScg, ACES2065-1, Linear Rec.709, Linear Rec.2020, SDR (sRGB) —
  one generated OCIO 2.4 config per preset, derived from the Blender 5.2 master config.
- **Identity import**: every config's Default file rule, `default` role and working space agree,
  so ALL footage imports as *nothing* — AE never guesses.
- **Single interpretation authority — the timeline**: footage is interpreted exclusively by
  per-layer `OCIO Color Space Transform` effects (source space → working), applied by the panel's
  interpret passes (per selection, or a recursive timeline walk with per-extension suggestions).
- **Self-contained projects**: the active config (plus its LUTs) is copied into
  `<project>/_minColor/` and the project is pinned to it. The panel's **Doctor** line diagnoses
  broken states (engine fallback, dead config path, foreign working space, footage-level
  assignments) and repairs in one click where possible.
- **View / Render adjustment layers**: one guide layer for viewport transforms (never renders),
  one render layer for delivery encoding (working → sRGB / Rec.1886 / …) — singletons that
  toggle each other.

## Layout

- `src/` — panel + libraries (`minColor.jsxinc` core, `AEPPatch.jsxinc` RIFX .aep reader/patcher)
- `config/` — `generate.py` builds `config/dist/` (per-preset configs, content-hashed filenames)
  from the vendored master in `config/master/`
- `build/build.py` — builds the distributable in `dist-panel/`: a single-file `minColor.jsx`
  plus its `minColor-data/` folder (configs, LUTs, presets, settings)

## Install

Build (or download) the distributable, then copy **both** items into your After Effects
**ScriptUI Panels** folder:

```
python3 build/build.py
# then copy dist-panel/minColor.jsx and dist-panel/minColor-data/ into:
#   macOS:   ~/Library/Preferences/Adobe/After Effects/<version>/Scripts/ScriptUI Panels/
#   Windows: %APPDATA%\Adobe\After Effects\<version>\Scripts\ScriptUI Panels\
```

Restart After Effects; the panel appears under **Window ▸ minColor.jsx**. The data
folder is invisible to AE's Window menu. That copy is the whole install — nothing else
runs or writes outside AE.

**Optional — the minColor plugin engine** (macOS/Apple silicon; Windows build pending):
copy `plugin-macOS/minColorCST.plugin` from the distributable into
`/Library/Application Support/Adobe/Common/Plug-ins/7.0/MediaCore/minColor/`, ideally with
the `configs` folder beside it (machine-wide store; projects fall back to sidecars without it).
The panel authors plugin effects automatically when it detects the installed bundle.

The panel finds its data next to itself first; a machine-wide copy at
`/Users/Shared/minColor` (or `C:\ProgramData\minColor`) acts as a facility-managed
fallback for studios that prefer centrally updated configs.
