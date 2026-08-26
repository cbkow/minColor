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
- `build/build.py` — single-file panel + payload in `dist-panel/`
- `build/install.command` — macOS install: payload → `/Users/Shared/minColor`,
  panel → the user-level ScriptUI Panels folder

## Install (macOS)

```
python3 build/build.py
build/install.command
```

Restart After Effects; the panel appears under **Window ▸ minColor.jsx**.
