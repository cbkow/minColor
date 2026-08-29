# Technical overview

What's under the hood. Nothing here changes how you use the panel.

## Identity import, single authority

Every config's Default file rule, `default` role and working space agree, so After Effects imports
all footage untransformed and never guesses. Interpretation happens once, on the timeline, as a
per-layer effect. Effect names are the durable state:

| name | meaning |
|---|---|
| `minColor: <space> → working` | interpret a footage layer |
| `minColor: view <space>` | the comp's guide layer (viewport only) |
| `minColor: render <space>` | the comp's render layer (working → delivery) |
| `minColor: look <name>` | an OCIO look before the transform on the same layer |
| `minColor: contain <space>` | treat a precomp's output as media in that space |

## Engine

`minColor CST` is a SmartFX effect that stores only a colourspace name and a direction. At render
it reads the project's OCIO config and working space from After Effects and processes with its own
statically linked OCIO 2.5.2. Its **passport** (config filename + working space, stored with the
effect) lets it resolve the config from the local store when the project's path is dead, so
renders match across platforms and under aerender. Adobe's native OCIO effect produces identical
pixels; **Package for any AE** translates to it.

## Configs

One config per preset, generated from a vendored Blender 5.2 master (AgX, ACES 1.3/2.0, cameras)
and down-levelled to OCIO 2.4 for After Effects. Filenames are content-hashed and the store is
append-only: a project pinned to a hash always finds it.

The SDR preset is display-referred: working space `Rec.709 Gamma 2.2`, Standard views only, no
looks, every colourspace defined with direct matrices. Platform views (`macOS Desktop View` = P3
primaries with a 2.2 curve, `macOS Video View` = the same after a BT.1886 encode and desktop sRGB
decode, `Windows Desktop View` = sRGB, `Windows Video View` = Rec.1886) and the two render targets
(`Desktop Render` = sRGB, `Video Render` = Rec.1886) exist in every preset.

The panel's menus follow the project's *pinned* config, never a newer one, and the Doctor reports
when an update is available.

## Where things live

| | macOS | Windows |
|---|---|---|
| Plug-in + config store | `/Library/Application Support/Adobe/Common/Plug-ins/7.0/MediaCore/minColor/` | `C:\Program Files\Adobe\Common\Plug-ins\7.0\MediaCore\minColor\` |
| Panel | `~/Library/Preferences/Adobe/After Effects/<ver>/Scripts/ScriptUI Panels/` | `<AE>\Support Files\Scripts\ScriptUI Panels\` |
| Settings | `/Users/Shared/minColor/settings/` | `C:\ProgramData\minColor\settings\` |
| Provenance | the project's XMP | the project's XMP |

## Archive and Package

**Archive** freezes dependencies next to the project (config + LUTs, `provenance.json`, a golden
reference frame future machines can diff against). **Package for any AE** additionally translates
every effect to Adobe-native and pins the sidecar, so the project opens on any After Effects 2025+
without minColor. Package a version increment, not your working copy.
