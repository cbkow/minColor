# Sidequest: Scripting-Driven, Pure-SDR OCIO Flow (no compiled plugin)

> **Superseded (panel 0.8.0, 2026-08-29).** The SDR idea shipped as the `SDR` preset (`sdr22`):
> a display-referred `Rec.709 Gamma 2.2` working space, pure Standard bridge, curated menus.
> See README → Model → *SDR preset*. This note is kept as the probe record that led there.

*Chris's note, folded in 2026-08-23. Original text first; the addendum at the end maps each point to
what the probes verified (see `probes/RESULTS.md`).*

---

Branches off the main AE/OCIO plugin notes. Triggered by confirming (via probe-color-scripting.jsx +
reflect.properties) that this AE build exposes color-management surface via ExtendScript beyond what's
in any published SDK doc. Undocumented / private-API territory — treat accordingly (see Risks).

**Why this is a different case from the ACES/ACEScg plugin plan.** The original compiled-plugin
architecture existed to solve one specific problem: AE's native OCIO engine likely lags Adobe's release
cadence, and newer OCIO/ACES features (ACES 2.0, OCIO 2.5 config constructs) may not be supported
natively. That's a real constraint for scene-linear ACES work. It is not a constraint for pure SDR
(Rec.709/sRGB gamma) work — that color science hasn't meaningfully changed, so "AE's bundled OCIO might
be a version behind" stops mattering. That removes the main reason we needed to statically link our own
OCIO and write a compiled effect at all.

**The shape.** Config — minimal custom OCIO config, SDR spaces only (Rec.709/sRGB gamma, source log
spaces if needed). No scene-linear role anywhere. Still load via a stable, never-moved file path +
project template. Bit depth — 8 or 16 bpc is fine. Input assignment — if the native OCIO Color Space
Transform effect's source-colorspace parameter reflects as a normal scriptable Property, the Sync
command can add the effect per bucket-matched layer via `addProperty(matchName)` and set its colorspace
param from the bucket table. Same bucket/DB/sync-command architecture: on-demand, keyed on persistent
item/layer IDs, diff-before-apply, never force-reorder an existing stack — only flag. Viewing — guide
layer at top of comp, native OCIO Display Transform effect, scripted target colorspace, excluded from
render via `AVLayer.guideLayer = true`. Delivery — Output Module color space set to the SDR target.

**What this eliminates vs. the full plugin plan.** No Smart Render / float buffer marshaling, no PF
pixel-format handling, no dynamic-popup UI problem, no static-linking OCIO, no PiPL / codesign /
notarize pipeline. "The plugin" is orchestration logic (ExtendScript / panel) driving effects and
properties that already exist in AE.

**Risks — undocumented surface.** Not in any published SDK doc; contradicted by Adobe's own community
replies; no changelog ⇒ no deprecation warning. Wrap every call in try/catch; keep the guide-layer /
project-template / manual-dialog fallback alive; degrade gracefully. Persistence bugs (config path
unreachable at open, not relative-path aware) are orthogonal to how the value got set.

**Next probe to run.** Add the native OCIO Color Space Transform to a test layer, reflect its colorspace
parameter: plain settable property, or opaque/arbitrary-data?

**Standing recommendation.** Have the Sync command re-run the reflection probe every time it runs.

---

## Addendum — status against the probes (2026-08-22/23)

| sidequest point | status |
|---|---|
| "Next probe": is the CST colourspace param a plain settable property? | **Yes, verified.** `ADBE OCIO Color Space Transform-0001/-0002` are popup properties; `setValue(index)` works; `propertyParameters` (26.0+) returns the label list (`role: Name` entries, a `(-` separator, then `Family/Name`); `valueText` reads back. Sync resolves names → index at apply time. Same for Display Transform (`Input Color Space`, `Display Device`, `View Transform`, `Inverse`). |
| Native effects need OCIO mode | Yes (refuse in Adobe mode); they **also run with working space None**, exact to OCIO math, unclamped float. |
| "8 or 16 bpc is fine" | **Correct.** The UI allows 16 bpc in OCIO mode (verified 2026-08-23); only the scripting setter `bitsPerChannel = 16` refuses (quirk) — New Project script sets 8 or 32, 16 is a manual flip (or a project-head patch later). With identity import no negatives/over-range arise in the SDR preset, so 16 bpc is a sound choice. |
| "No scene-linear role anywhere" | Not needed: the working space is just the viewer/OM label once import is identity. The SDR flavour is **working = a display-referred space** (e.g. `Gamma 2.4 Encoded Rec.709` or `sRGB`), Default rule == that space, `default` role == that space ⇒ identity import, viewer sRGB-of-sRGB ≈ identity, OM identity. Compositing on gamma-encoded values, as classic AE. |
| Project-level config by script; working space | Config: `ocioConfigurationFile` (absolute path). Working space: **not settable live** (setter destroys it) — use the sticky pref for new projects and `aep_patch` for existing ones; both verified. |
| Input assignment by per-layer effect | Works; **alternative now available**: assign at the footage level via `file_rules` (import) or `.aep` patch (existing items) — no stack-ordering problem, visible in Project panel, readable back via `ocsp`. Effects remain for overrides/looks/LUTs. |
| Viewing via guide layer + native Display Transform | Verified exact. With a working space set, AE's own viewer Display Color Space also works (pending eyes) — guide layer optional. |
| Delivery via OM colour space | OM default output = config `default` role; templates for the rest. |
| Undocumented surface / regression probe | Agreed: Sync pre-flight runs `probe-color-scripting.jsx`-style reflection each time and asserts `workingSpace !== "None"`. |

**Conclusion:** the sidequest is not a separate flow any more — it is a **working-space preset** of the
same scripting-driven v0 (see `PREPLAN.md` §3.7). The main plan itself became "no compiled plugin
until GPU/2.5 needs prove out", so both converge.
