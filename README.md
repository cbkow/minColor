# minColor

An OCIO colour-management pipeline for After Effects: a dockable ScriptUI panel plus a
compiled plugin engine (macOS and Windows).

Panel 0.9.0 · engine 1.3.1 (Windows prebuilt 1.3.0 until rebuilt) · requires After Effects 2025+.

## Components

- **Panel** (`minColor.jsx` + `minColor-data/`) — project setup/migration, footage and
  timeline interpretation, precomp containment, OCIO-effect stripping, view/render
  adjustment layers, looks, and a Doctor status line that diagnoses and repairs project
  colour state.
- **Plugin engine** (`minColor CST`, matchName `MINC CST`) — a SmartFX effect that stores
  only a colourspace name and direction. At render it resolves the project's current OCIO
  config and working space and processes with its own statically linked OCIO 2.5.2.
- **Config family** — one OCIO config per working-space preset, generated from a vendored
  master. Four linear presets (ACEScg, ACES2065-1, Linear Rec.709, Linear Rec.2020) and one
  SDR preset (`Rec.709 Gamma 2.2` working space). Filenames are content-hashed; the hash is
  the config's identity.

## Model

- **Identity import**: each config's Default rule, `default` role and working space agree,
  so all footage imports untransformed. AE never guesses.
- **The timeline is the only interpretation authority**: interpretation is per-layer
  minColor effects. Effect display names are the durable store; the plugin's
  "Sync From Names" command derives effect state from them.
- **Effect grammar**: `minColor: <space> → working` (interpret footage) ·
  `minColor: view <space>` · `minColor: render <space>` (utility layers) ·
  `minColor: look <name>` (OCIO look, applied before the transform on the same layer) ·
  `minColor: contain <space>` (a precomp's output is media in that space; interpret
  passes do not descend into it — name-level only, no panel UI).
- **Interpret timeline** also gives the comp both utility layers from the panel's current View
  and Render choices, with the view enabled.
- **SDR preset**: the working space is display-referred (`Rec.709 Gamma 2.2` — Rec.709
  primaries, pure 2.2, what a desktop display does). SDR is SDR: the working values are the
  deliverable, Rec.709 video is working-native (no interpret effect), sRGB stills get the exact
  sRGB→2.2 re-encode, linear/log sources get plain inverse-OETF conversions (no tone mapping —
  that is what the linear presets are for). Only Standard/Raw views exist and there are no
  looks; tone-mapped view/render targets are absent because on display-referred pixels they
  would be double transforms. The render layer is explicit (the panel preselects the identity;
  `Rec.1886`, `sRGB`, `Display P3`, `Rec.2020` are deliberate re-encodes for a specific consumer,
  and look lifted/darkened in the viewport by construction). `macOS Desktop View` (P3 primaries, pure 2.2 — AE's macOS viewport surface) and
  `macOS Video View` (the same after a BT.1886 encode + desktop sRGB decode: how a 709
  delivery plays through QuickTime-style pipelines) are view targets only, never render targets.
  `Windows Desktop View` (= sRGB) and `Windows Video View` (= Rec.1886) are the Windows
  counterparts; `Desktop Render` (= sRGB) and `Video Render` (= Rec.1886) are the render targets.
  Platform views head the View list, the two Renders head the Render list, and the first-use
  defaults are the platform's own Video view with `Video Render` (the panel remembers later choices). The older `macOS View Only` display stays in every config (AE
  stores the viewer's display choice by name) but is no longer offered; effects carrying it are
  renamed on migrate.
- **Central config store**: projects pin the config beside the plugin in Adobe's shared
  MediaCore folder. Per-project sidecars are produced only by "Package for any AE".
- **Passport**: each effect's sequence data carries the config's hashed filename and the
  working space. If the project's pin is dead (typically after crossing platforms), the
  plugin resolves the config from the local store and renders identically — including
  under aerender. The panel additionally re-pins the project and records the repair.
- **Provenance**: preset, config identity and tool versions are stamped into the project's
  XMP. Repairs and migrations key off it; projects without it are never touched.
- **User settings** (dropdown state, extension table, repair history) live outside the
  install and survive updates: `/Users/Shared/minColor/settings/` on macOS,
  `C:\ProgramData\minColor\settings\` on Windows.

## Install

Use a release zip: two copy steps, described in its `README.txt` — the panel pair into
ScriptUI Panels, and the platform's `minColor` folder into
`…/Adobe/Common/Plug-ins/7.0/MediaCore/`.

## Build from source

```
python3 config/generate.py          # config family -> config/dist/
plugin/external/build.sh            # static OCIO 2.5.2 (build.bat on Windows)
cmake -S plugin -B plugin/build && cmake --build plugin/build
python3 build/build.py              # distributable + zip -> dist-panel/
```

`plugin/scripts/dev-install.sh|.bat` installs the built plugin (and, on Windows, the
config store); `dev-install-panel.bat` builds and installs the panel. The Windows release
`.aex` is committed at `plugin/prebuilt/windows/` and bundled by `build.py`.

## Layout

- `src/` — panel and libraries (`minColor.jsxinc` core, `AEPPatch.jsxinc` RIFX reader/patcher)
- `config/` — `generate.py`, vendored master, generated `dist/`
- `plugin/` — effect/AEGP sources, CMake build, PiPL, probe tool
- `build/` — distributable build
