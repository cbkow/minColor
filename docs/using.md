# Using minColor

minColor 2.0 lives in three places that are really one thing:

- **Effects on your layers** — the interface you'll touch most. Every minColor effect has a **Space**
  dropdown in Effect Controls; change it to set that effect's color space in place.
- **Menu commands** — every ceremony (Migrate, Interpret, Utility Layers, Apply Look, Apply Render
  Preset, Strip, Doctor/Repair…) is a native After Effects menu command, scriptable and shortcut-able.
- **The panel** (Window → minColor.jsx) — a thin dashboard over those commands: a Doctor status
  line, the ceremonies as buttons, and dropdowns fed by your project's config. It's passive — it
  acts only when you click.

The panel needs the plug-ins: if the engine isn't installed (or is older than the panel), the
panel says so and stays out of the way.

## Status

The Doctor line at the top of the panel. Green shows the preset, working space, and where the
config is pinned. Yellow with a repair action means the config path went missing (usually a
project that came from another machine) — the panel heals it in place, and **minColor: Repair**
does the same from the menu. Yellow "update available" means the project is pinned to an older
config of its preset; Migrate to the same preset updates it. Red spells out what to fix by hand.

## Migrate

**Migrate Project** — choose a preset:

- Linear presets (`ACEScg, ACES2065-1, Linear Rec.709, Linear Rec.2020`) for CG, VFX and HDR work.
- **SDR** (`Rec.709 Gamma 2.2`) for an OCIO workflow similar to Adobe color space.

Migrate is the single managed-project entry (on a fresh project it just sets things up). It switches
the project's config **live — no reopen** — sets the working space, remaps existing minColor effects
to the new preset (removing ones that become identity), and gives the open comp its view and render
layers, backing up the `.aep` to `_minColor` first. Projects made with minColor 1.x migrate the same
way: their old effects are rebuilt as the current ones, names and positions kept.

## Interpret

**Interpret Timeline** walks the active comp and every precomp under it, interpreting each
footage layer. Suggestions come from your extension table first (the panel's **Matches…**
edits it), then from what the project used before, then from file metadata; plain video
containers fall back to a Rec.709 read. Each layer gets a `minColor: <space> → working` effect.

**Interpret Selected** does the same for just the selected layers — pick a specific space in
the panel to override the suggestion.

**Strip Foreign OCIO** removes OCIO effects that aren't minColor's; **Strip ALL** removes every
OCIO effect. Both are undoable.

## Adjustment Layers

**Utility Layers** builds the pair on the active comp. *View* is a guide layer for the viewport
only. *Render* is what your output goes through; only one of the two is on at a time. The
layers always match the comp — size, pixel aspect, the lot — even after you resize the comp.

- `macOS Desktop View` / `Windows Desktop View` — what the working values look like on that
  platform's screen.
- `macOS Video View` / `Windows Video View` — how a Rec.709 video delivery will look.
- `Desktop Render` (sRGB) / `Video Render` (Rec.1886) — the two deliveries.

**Apply Look** (linear presets) applies an OCIO look, such as ACES gamut compression, on both
layers. **Apply Render Preset** sets view, render and look in one click from your saved recipes
(`settings/render-presets.json`).

## What each effect remembers

Every minColor effect stores its color space in its own saved parameter (the **Space** dropdown),
and renders straight from it — no names to keep in sync, no background bookkeeping. The display name
mirrors the choice (`minColor: ARRI LogC4 → working`, `minColor: view macOS Video View`) so the
timeline stays readable, but the saved parameter is what renders, which is why a project renders
correctly on a farm with nothing installed but the effect.

If you rename effects by hand and want the saved parameters brought back in line with the names,
**Sync From Names** re-derives them on demand — a rescue for hand-edited or older projects.
