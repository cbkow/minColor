# Third-Party Notices & Credits

minColor is built on, and ships transforms derived from, the open-source projects and colour-science
work below. This file collects their attributions. (minColor's own licence is stated in `LICENSE`.)

---

## Colour engine — OpenColorIO

minColor statically links **OpenColorIO 2.5.2** (the colour-transform engine) into both plug-ins.

- Project: <https://opencolorio.org>
- Licence: **BSD-3-Clause** — *"Copyright Contributors to the OpenColorIO Project."*
- Full text bundled at `plugin/external/src/OpenColorIO/LICENSE`.

OpenColorIO in turn bundles and links a number of libraries. Their authoritative licence texts are in
OpenColorIO's own third-party notice, bundled at `plugin/external/src/OpenColorIO/THIRD-PARTY.md`.
The principal linked dependencies and their licence families are:

| Dependency | Licence |
|---|---|
| Imath | BSD-3-Clause |
| pystring | BSD-3-Clause |
| yaml-cpp | MIT |
| expat | MIT |
| minizip-ng | zlib / BSD |
| zlib | zlib |
| (OpenColorIO-vendored: Half, SampleICC, xxHash, …) | see OpenColorIO `THIRD-PARTY.md` |

Refer to OpenColorIO's `THIRD-PARTY.md` for the complete, authoritative list and full licence texts.

---

## Colour configurations, view transforms & LUTs

minColor's OCIO configurations (`config/`) and the LUTs it embeds (`luts/`, `filmic/`, `icc/`) are
generated/derived from:

- **Blender colour management** (Blender 5.2), including the **AgX** and **Filmic** view transforms
  authored by **Troy Sobotka**. Source: the Blender project's OCIO configuration.
  <https://www.blender.org>
- **ACES — Academy Color Encoding System** (2.0 / 1.3) transforms.
  © Academy of Motion Picture Arts and Sciences (A.M.P.A.S.), provided under the ACES licence.
  <https://acescentral.com>

These are used under their respective upstream licences; consult each project for the exact terms
before redistribution.

---

## Adobe After Effects SDK

Both plug-ins are built against Adobe's After Effects SDK (© Adobe Inc.). The SDK headers and example
sources are **not** redistributed in this repository.

---

## Credits & inspiration

minColor's core design — assigning colour spaces **per layer, with OCIO effects on the timeline**,
rather than through After Effects' *Interpret Media* — follows the path blazed by **Brendan Bolles'
fnordware OpenColorIO plug-in for After Effects**. No fnordware code is used in minColor; the debt is
one of ideas, and it is gratefully acknowledged.

- Brendan Bolles / fnordware — <https://www.fnord.com>

With thanks also to the OpenColorIO project, the Academy (ACES), and Troy Sobotka (AgX / Filmic),
whose work makes a colour-managed After Effects workflow possible.
