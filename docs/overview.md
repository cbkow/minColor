# Technical overview

For the curious

## Media comes in untouched

Every config is set up so After Effects imports footage without converting it: the default file
rule, the `default` role and the working space all agree. Color spaces are assigned on the
timeline instead, one effect per layer, named for what they do:

| effect | example name | what it does |
|---|---|---|
| minColor Transform | `minColor: ARRI LogC4 → working` | interprets a footage layer |
| minColor View | `minColor: view macOS Video View` | the comp's View guide layer (viewport only) |
| minColor Render | `minColor: render Video Render` | the comp's Render layer (what gets output) |
| minColor Look | `minColor: look <name>` | an OCIO look, applied before the transform |

The names are the truth. Rename an effect — or click its badge and pick from the menu — and the
plugin follows. Which effect it is (its match name) says what it's *for*; the display name says
which space it uses.

## The plug-ins

Two bundles, one build, one version:

- **minColorCST.plugin** (MediaCore) carries the effects. Each stores only a color space name
  and a direction; when it renders it asks After Effects for the project's OCIO config and
  working space, then runs the transform with its own built-in OCIO 2.5.2. It also remembers
  the config it was last synced with (the "passport"), so a project that arrives on a machine
  where the config path doesn't exist still renders the same — in the app and in aerender.
  Adobe's own OCIO Color Space Transform gives identical pixels; **Package for Any AE** swaps
  every effect over to it.
- **minColorAEGP.plugin** (each After Effects' own Plug-ins folder) carries the ceremonies as
  menu commands, watches the project to keep effect names and settings in sync, and writes the
  handshake file the panel checks at startup (`settings/aegp-api.json`).

Projects made with minColor 1.x open fine: their `MINC CST` effects appear as placeholders and
**Migrate Project** rebuilds each one as the current effect with the same name and position.

## The configs

Generated from a Blender 5.2 config plus ACES 2.0 pieces, then trimmed to OCIO 2.4 because that's
what After Effects ships with. Each preset is its own config file, named by a hash of its
contents. Old versions are never deleted: a project always finds the config it was set up with,
and the Doctor tells you when a newer one is available.

The SDR config uses a Rec. 709 gamma 2.2 working space with plain matrix and curve transforms —
no tone mapping. The platform views (`macOS Desktop View`, `macOS Video View`, and the Windows
pair) exist so the viewport shows the right thing on each OS; `Desktop Render` and `Video Render`
are sRGB and Rec. 1886 outputs.

## Where things live

| | macOS | Windows |
|---|---|---|
| Effects + configs | `/Library/Application Support/Adobe/Common/Plug-ins/7.0/MediaCore/minColor/` | `C:\Program Files\Adobe\Common\Plug-ins\7.0\MediaCore\minColor\` |
| Ceremonies (mac) | `/Applications/Adobe After Effects <ver>/Plug-ins/minColorAEGP.plugin` | — until the 2.0 engine lands on Windows |
| Panel | `~/Library/Preferences/Adobe/After Effects/<ver>/Scripts/ScriptUI Panels/` | `%APPDATA%\Adobe\After Effects\<ver>\Scripts\ScriptUI Panels\` |
| Your settings | `/Users/Shared/minColor/settings/` | `C:\ProgramData\minColor\settings\` |

## Archive and Package

**Archive** copies the project's config and LUTs next to the project file, with a note of the
versions used. **Package for Any AE** does that and converts every effect to Adobe-native, so the
project opens on any After Effects 2025+ without minColor installed. Package a version increment,
not your working copy.
