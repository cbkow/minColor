# minColor

**minColor** is OCIO colour management for After Effects: a dockable panel and a compiled plug-in
engine, on macOS and Windows. Footage imports untouched, interpretation lives on the timeline as
named effects, and the colour engine travels with the project — so a comp renders the same on
every machine, now and years from now.

Panel 0.9.1 · engine 1.3.1 · After Effects 2025 or later.

## Install

### macOS

Download `minColor-<version>.pkg` from [GitHub Releases](https://github.com/cbkow/minColor/releases/latest),
quit After Effects, and run it. Signed and notarized.

### Windows

Download `minColor-<version>.msi`, quit After Effects, and run it. The installer is unsigned, so
Windows asks once per machine.

Both installers put the plug-in and config store in Adobe's shared MediaCore folder and the panel
in every After Effects 2025+ on the machine. Updates install over the previous version and keep
your settings. Then open **Window → minColor.jsx**.

A zip is also attached to each release for manual copying; its `README.txt` has the two paths.

## What it does

- **Presets** — four scene-linear working spaces (ACEScg, ACES2065-1, Linear Rec.709, Linear
  Rec.2020) and **SDR** (`Rec.709 Gamma 2.2`) for display-referred motion graphics.
- **Interpret** — per-layer OCIO effects named for what they do (`minColor: ARRI LogC4 → working`),
  suggested from your extension table, the project's history, or file metadata.
- **View and Render layers** — platform-aware previews (`macOS Desktop View`, `macOS Video View`,
  Windows equivalents) and named render targets (`Desktop Render`, `Video Render`).
- **Doctor** — a status line that knows the project's preset, config and working space, repairs a
  broken config path in one click, and tells you when a config update is available.
- **Migrate** — moves an existing project into the pipeline in one save/backup/reopen.
- **Archive / Package** — freeze a project's colour dependencies next to it, or hand it to someone
  without minColor.
- **Engine** — the plug-in pins its own OCIO 2.5.2 and carries a passport, so renders are
  identical across platforms and under aerender even when the config path is dead.

| | macOS | Windows |
|---|---|---|
| **Panel** | ScriptUI (dockable) | ScriptUI (dockable) |
| **Engine** | `.plugin`, arm64, notarized | `.aex`, x64 |
| **Configs** | OCIO 2.4 (AE's embedded OCIO), generated from Blender 5.2 + ACES | same |
| **Installer** | signed `.pkg` | `.msi` |

## Documentation

- [Using minColor](docs/using.md)
- [Technical overview](docs/overview.md)
- [Building and releasing](docs/building.md)
- [How the SDR preset came about](docs/sidequest-sdr-scripting.md)
