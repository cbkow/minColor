# minColor

OCIO colour management for After Effects: a dockable panel plus a compiled plug-in engine, on
macOS and Windows. Projects import everything untransformed, interpretation lives on the timeline
as named effects, and the colour engine is pinned so a project renders the same on every
machine — including years later.

Panel 0.9.1 · engine 1.3.1 · After Effects 2025 or later.

## Install

Download the release for your platform:

- **macOS** — `minColor-<ver>.pkg`. Quit After Effects, run the installer. It places the plug-in
  and config store in Adobe's shared MediaCore folder, the panel in every After Effects 2025+ you
  have launched, and seeds settings under `/Users/Shared/minColor/settings`. Signed and notarized;
  nothing to approve beyond the normal installer prompts. Remove with
  `packaging/macos/uninstall.command` from the repository.
- **Windows** — `minColor-<ver>.msi`. Quit After Effects, run it (or `msiexec /i minColor-<ver>.msi /qn`
  for silent deployment). It is unsigned, so Windows shows one "unknown publisher" prompt per
  machine. Uninstall from Add/Remove Programs.
- **Zip** (either platform, manual): copy `minColor.jsx` + `minColor-data/` into your ScriptUI
  Panels folder and the platform's `minColor` folder into
  `…/Adobe/Common/Plug-ins/7.0/MediaCore/` — details in the zip's `README.txt`.

Then start After Effects and open **Window → minColor.jsx**. Updates install over the previous
version; your settings and every config a project might be pinned to are kept.

## Use

**Set Up / Migrate Project…** — pick a preset and either create a **New Project** or **Migrate
Current**. Presets are working spaces: four linear ones (ACEScg, ACES2065-1, Linear Rec.709,
Linear Rec.2020) for CG, VFX and HDR work, and **SDR** (`Rec.709 Gamma 2.2`) for display-referred
motion graphics where the working values are the deliverable. Migrate strips any footage-level
colour assignments (keeping them as suggestions), sets the working space and config, rebuilds
minColor effects, and gives the open comp its view and render layers — one save, backup and reopen.

**Interpret Footage** — *Selected as:* a colourspace, **Apply**: the selected layers get a
`minColor: <space> → working` effect.

**Interpret Timeline** — **Interpret timeline** walks the active comp and its precomps, suggesting a
space per item (your extension table first — **Matches…** edits it — then what the project had
before, then detected metadata) and finishes by putting the view and render layers on the comp.
In SDR, Rec.709 video is left untouched: it already *is* the working space. **Strip foreign OCIO**
removes non-minColor OCIO effects that would fight the pipeline; **Strip ALL** removes every OCIO
effect (undoable).

**Adjustment Layer** — *View* is a guide layer for the viewport only; *Render* is what your output
goes through. Choose and **Apply**. Views: `macOS Desktop View` / `Windows Desktop View` show what
the numbers mean on that platform's screen; `macOS Video View` / `Windows Video View` preview how a
Rec.709 video delivery will play back. Renders: `Desktop Render` (sRGB) or `Video Render`
(Rec.1886). Only one of view/render is enabled at a time; the panel remembers your choices. *Look*
(linear presets) sets an OCIO look on both layers.

**Doctor** — the status line at the top. Green: preset, working space and where the config is
pinned. Yellow with **Repair**: the config path went missing (usually a project that crossed
platforms) — one click re-points it. Yellow "update available": your project is pinned to an
older config of its preset; Migrate to the same preset updates it. Red: something to fix by hand,
spelled out in the message. The gear opens **Project…** with provenance, **Archive** (freeze
dependencies next to the project) and **Package for any AE** (hand the project to someone without
minColor).

## Going deeper

- [docs/architecture.md](docs/architecture.md) — the model: identity import, single authority,
  the config family and the SDR preset, pins, passports, Archive vs Package, the plug-in.
- [docs/building.md](docs/building.md) — building from source, versions, signing and
  notarization, the installers, the release checklist, verification tools.
- [docs/sidequest-sdr-scripting.md](docs/sidequest-sdr-scripting.md) — the probe record that led
  to the SDR preset.
