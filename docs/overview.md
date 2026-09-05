# Technical overview

For the curious

## Media comes in untouched

Every config is set up so After Effects imports footage without converting it: the default file rule, the `default` role and the working space all agree. Color spaces are assigned on the timeline instead, one effect per layer, named for what they do:

| effect | example name | what it does |
|---|---|---|
| minColor Transform | `minColor: ARRI LogC4 → working` | interprets a footage layer |
| minColor View | `minColor: view macOS Video View` | the comp's View guide layer (viewport only) |
| minColor Render | `minColor: render Video Render` | the comp's Render layer (what gets output) |
| minColor Look | `minColor: look <name>` | an OCIO look, applied before the transform |

Each effect stores its color space in its own saved parameter — pick it from the **Space** dropdown in Effect Controls. The display name mirrors that choice (which effect it is, its *match name*, says what it's *for*; the display name says which space it uses), so the timeline reads clearly, but the saved parameter is the truth: the effect renders from it with no external state to consult.

## The plug-ins

Two bundles, one build, one version:

- **minColorCST** (MediaCore) carries the effects. Each stores a color space name and a direction, and renders with its own built-in OCIO 2.5.2 from a **copy of the config baked into the binary** — it does not read AE's project config or any file on disk. It knows which config to use from its "passport" (the config's basename), so a project renders the same wherever it lands — in the app and in `aerender` — even where no config files exist.
- **minColorAEGP** (each After Effects' own Plug-ins folder) carries the ceremonies as menu commands, writes the handshake file the panel checks at startup (`settings/aegp-api.json`), and seeds the panel's settings from its own embedded copies on first launch. It runs only when you invoke a command — nothing watches or rewrites the project in the background.

Projects made with minColor 1.x open fine: their `MINC CST` effects appear as placeholders and **Migrate Project** rebuilds each one as the current effect with the same name and position.

## The configs

Generated from a Blender 5.2 config plus ACES 2.0 pieces, then trimmed to OCIO 2.4 because that's what After Effects ships with. Every preset's full config **and its LUTs are embedded in the effect binary** (and the metadata in the AEGP), so nothing is installed to disk.

After Effects still needs an OCIO config to be in a managed state, so Migrate writes a lean per-preset **interface config** into a `_minColor` folder beside the project and points AE at it. That config exposes only the preset's working space (its `scene_linear`) and a passthrough view — a neutralizer. AE composites in that space; the minColor effects own all the real color, from their embedded configs. The Doctor tells you when a newer preset config is available (Migrate, same preset, updates it).

The SDR config uses a Rec. 709 gamma 2.2 working space with plain matrix and curve transforms — no tone mapping. The platform views (`macOS Desktop View`, `macOS Video View`, and the Windows pair) exist so the viewport shows the right thing on each OS; `Desktop Render` and `Video Render` are sRGB and Rec. 1886 outputs.

## Where things live

| | macOS | Windows |
|---|---|---|
| Effect | `/Library/Application Support/Adobe/Common/Plug-ins/7.0/MediaCore/minColor/` | `C:\Program Files\Adobe\Common\Plug-ins\7.0\MediaCore\minColor\` |
| Ceremonies (AEGP) | `/Applications/Adobe After Effects <ver>/Plug-ins/minColorAEGP.plugin` | `…\Adobe After Effects <ver>\Support Files\Plug-ins\minColorAEGP.aex` |
| Panel | `~/Library/Preferences/Adobe/After Effects/<ver>/Scripts/ScriptUI Panels/` | `%APPDATA%\Adobe\After Effects\<ver>\Scripts\ScriptUI Panels\` |
| Your settings | `/Users/Shared/minColor/settings/` | `C:\ProgramData\minColor\settings\` |

No config store is installed — the configs live in the binaries. The settings folder is seeded by the AEGP at first launch (your own edits there survive updates).

## The `_minColor` sidecar

Migrate writes a `_minColor` folder beside the project: the lean interface config AE pins, and a timestamped `.aep` backup taken before any change. It travels with the project, and holds no LUTs or full configs — the effect carries those.
