# Technical overview

What's going on behind the panel. You don't need any of this to use it.

## Media comes in untouched

Every config is set up so After Effects imports footage without converting it: the default file
rule, the `default` role and the working space all agree. Color spaces are assigned on the
timeline instead, one effect per layer, named for what they do:

| effect name | what it does |
|---|---|
| `minColor: ARRI LogC4 → working` | interprets a footage layer |
| `minColor: view macOS Video View` | the comp's View guide layer (viewport only) |
| `minColor: render Video Render` | the comp's Render layer (what gets output) |
| `minColor: look <name>` | an OCIO look, applied before the transform |

The names are the truth. Rename an effect and the plugin follows.

## The plugin

`minColor CST` stores only a color space name and a direction. When it renders it asks After
Effects for the project's OCIO config and working space, then runs the transform with its own
built-in OCIO 2.5.2. It also remembers the config it was last synced with (the "passport"), so if
a project arrives on a machine where the config path doesn't exist, it still renders the same —
in the app and in aerender. Adobe's own OCIO Color Space Transform gives identical pixels;
**Package for any AE** swaps every effect over to it.

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
| Plugin + configs | `/Library/Application Support/Adobe/Common/Plug-ins/7.0/MediaCore/minColor/` | `C:\Program Files\Adobe\Common\Plug-ins\7.0\MediaCore\minColor\` |
| Panel | `~/Library/Preferences/Adobe/After Effects/<ver>/Scripts/ScriptUI Panels/` | `<After Effects>\Support Files\Scripts\ScriptUI Panels\` |
| Your settings | `/Users/Shared/minColor/settings/` | `C:\ProgramData\minColor\settings\` |

## Archive and Package

**Archive** copies the project's config and LUTs next to the project file, with a note of the
versions used. **Package for any AE** does that and converts every effect to Adobe-native, so the
project opens on any After Effects 2025+ without minColor installed. Package a version increment,
not your working copy.
