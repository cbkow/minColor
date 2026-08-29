#!/usr/bin/env python3
"""minColor config generator.

master/ (vendored Blender 5.2 colormanagement, OCIO 2.5)
  + QCView EDR displays (patches/blender52-edr.patch)
  + `macOS View Only` display (P3 primaries, pure 2.2 — AE's mac viewport surface) with the
    1886-sim as a ViewTransform
  + per-preset Default file rule + roles (identity import: Default rule == working == `default` role)
  + OCIO 2.4 down-level for AE (version tag, interop_id, interchange blocks)

Two preset families:
  Linear  (acescg, ap0, lin709, lin2020): scene-linear working space; the master's colourspaces,
          displays, views and looks as-is.
  SDR     (sdragx, sdraces2, sdraces13, sdrfilmic): DISPLAY-REFERRED working space
          `Rec.709 Gamma 2.2`. OCIO inserts `default_view_transform` on every scene<->display
          ColorSpaceTransform, so the preset's rendering (AgX / ACES 2.0 / ACES 1.3 / Filmic) is
          applied ONCE, on ingest, to scene-referred sources (linear EXR, camera log) while
          display-referred sources (sRGB, Rec.1886, Display P3) stay pure. That only works when
          every colourspace / view transform on the path is SELF-CONTAINED: the master's display
          spaces and Linear Rec.709/2020/P3/E-Gamut hop through `Linear CIE-XYZ D65` (the
          cie_xyz_d65_interchange role), which OCIO resolves as a second reference crossing — the
          bridge then cancels (inverse∘forward) or recurses (AgX/Khronos segfault). The SDR branch
          rewrites those with direct matrices, keeps only Standard/Raw views (a tone-mapped view on
          display-referred pixels is a double transform) and drops looks.

dist/
  luts/ filmic/ icc/                    (shared, copied once)
  config-master-2.5.ocio                (QCView / Blender / tooling)
  config-<preset>-<hash>.ocio           (AE; content-hashed, APPEND-ONLY — never delete old ones)
  presets.json                          (per-preset working/label/menus + sticky-pref PwCs JSON)

Validation: ociocheck + PyOpenColorIO load/validate + file-rule resolution (both path separators)
+ DisplayViewTransform build for every listed display/view. SDR configs additionally run
`validate_sdr` in a SUBPROCESS (a nested-reference regression segfaults OCIO): bridge CST ==
genuine DisplayViewTransform, rewrite fidelity vs the master's original views, display purity, and
text asserts that no nested reference crossing survived.
NOTE: a 2.5 lib cannot prove 2.4 compliance — AE's `ocioConfigurationFile` setter is the final judge.
"""
import base64
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
MASTER = os.path.join(HERE, "master")
DIST = os.path.join(HERE, "dist")

SDR_WORKING = "Rec.709 Gamma 2.2"
MAC_VIEW = "macOS View Only"                                          # LEGACY key: kept in every config — AE's viewer stores its
MAC_VIEW_SIM = "macOS View Only (1886 Sim)"                           # display choice per project BY NAME (renaming resets it)
MAC_DESKTOP = "macOS Desktop View"                               # same transform as MAC_VIEW (P3 primaries, pure 2.2); the name going forward
MAC_VIDEO = "macOS Video View"                                   # MAC_DESKTOP after a BT.1886 encode + desktop sRGB decode (QuickTime-style playback of a 709 delivery)
WIN_DESKTOP = "Windows Desktop View"                             # == sRGB (a Windows desktop surface)
WIN_VIDEO = "Windows Video View"                                 # == Rec.1886 (a 709 video delivery on a 2.4 display)
DESKTOP_RENDER = "Desktop Render"                                     # == sRGB   (render target for desktop/web deliveries)
VIDEO_RENDER = "Video Render"                                         # == Rec.1886 (render target for video deliveries)
PLATFORM_VIEWS = [MAC_DESKTOP, MAC_VIDEO, WIN_DESKTOP, WIN_VIDEO]     # top of every View list
RENDER_ONLY = [DESKTOP_RENDER, VIDEO_RENDER]                          # top of every Render list; never views, never inputs

# preset key -> record. Keys must be ALPHANUMERIC: the panel parses the key out of the hashed
# filename (CONFIG_PATTERN /config-([A-Za-z0-9]+)-[0-9a-f]+\.ocio$/). Order = Set Up dialog order.
PRESETS = {
    "acescg":    {"working": "ACEScg",          "family": "Linear",  "label": "Linear \u00b7 ACEScg",     "bridge": None},
    "ap0":       {"working": "ACES2065-1",      "family": "Linear",  "label": "Linear \u00b7 ACES2065-1", "bridge": None},
    "lin709":    {"working": "Linear Rec.709",  "family": "Linear",  "label": "Linear \u00b7 Rec.709",    "bridge": None},
    "lin2020":   {"working": "Linear Rec.2020", "family": "Linear",  "label": "Linear \u00b7 Rec.2020",   "bridge": None},
    # SDR is SDR (Chris, 2026-08-29): ONE display-referred preset, pure Standard bridge (matrix only,
    # no tone mapping) — AE's native gamma compositing with minColor's menus and doctrine.
    "sdr22":     {"working": SDR_WORKING, "family": "Display", "label": "SDR", "bridge": "Standard"},
}
# Retired presets: no longer offered, but projects pinned to them stay recognised (Archive/Package
# need the config basename). Their hashed files stay on disk forever (append-only store).
RETIRED = {
    "sdr": {"config": "config-sdr-0a1f224e.ocio", "working": "sRGB", "family": "Display",
            "workingSpaceLabel": "Display/sRGB", "label": "SDR (retired \u2014 raw sRGB)"},
    # the tone-mapped-ingest experiments of 2026-08-29 (kept on disk: append-only store)
    "sdragx":    {"config": "config-sdragx-75d77479.ocio",    "working": SDR_WORKING, "family": "Display", "workingSpaceLabel": "Display/" + SDR_WORKING, "label": "SDR \u00b7 AgX (retired)"},
    "sdraces2":  {"config": "config-sdraces2-37ea85c4.ocio",  "working": SDR_WORKING, "family": "Display", "workingSpaceLabel": "Display/" + SDR_WORKING, "label": "SDR \u00b7 ACES 2.0 (retired)"},
    "sdraces13": {"config": "config-sdraces13-fe5745cb.ocio", "working": SDR_WORKING, "family": "Display", "workingSpaceLabel": "Display/" + SDR_WORKING, "label": "SDR \u00b7 ACES 1.3 (retired)"},
    "sdrfilmic": {"config": "config-sdrfilmic-6d0c51fb.ocio", "working": SDR_WORKING, "family": "Display", "workingSpaceLabel": "Display/" + SDR_WORKING, "label": "SDR \u00b7 Filmic (retired)"},
}

UNION_VIEW_TRANSFORM = """  - !<ViewTransform>
    name: Standard 1886 Sim
    description: |
      Standard display transform preceded by a BT.1886 (2.4) encode + sRGB decode in linear Rec.709 —
      simulates broadcast-authored content played through an sRGB-decoding pipeline. Expressed as a
      ViewTransform (not a look) because AE's native OCIO Display Transform effect silently passes
      through on views that reference looks (verified 2026-08-26).
    from_scene_reference: !<GroupTransform>
      children:
        - !<ColorSpaceTransform> {src: ACES2065-1, dst: Linear Rec.709}
        - !<ExponentTransform> {value: 2.4, direction: inverse}
        - !<ExponentWithLinearTransform> {gamma: 2.4, offset: 0.055}
        - !<ColorSpaceTransform> {src: Linear Rec.709, dst: ACES2065-1}
        - !<BuiltinTransform> {style: UTILITY - ACES-AP0_to_CIE-XYZ-D65_BFD}
"""

MAC_DISPLAY_CS = """
  - !<ColorSpace>
    name: %s
    aliases: [UnionMacOS]
    family: Display
    equalitygroup: ""
    bitdepth: 32f
    description: |
      Display P3 primaries with pure 2.2 EOTF — matches Apple's effective display pipeline when the
      viewport surface is P3-tagged with EDR off (AE macOS viewport). VIEW ONLY: never a render target.
    isdata: false
    encoding: sdr-video
    from_display_reference: !<GroupTransform>
      children:
        - !<ColorSpaceTransform> {src: cie_xyz_d65_interchange, dst: Linear DCI-P3 D65}
        - !<ExponentTransform> {value: 2.2, direction: inverse}
""" % MAC_VIEW

MAC_VIEWS = """  %s:
    - !<View> {name: Standard, view_transform: Standard, display_colorspace: %s}
    - !<View> {name: Standard 1886 Sim, view_transform: Standard 1886 Sim, display_colorspace: %s}
    - !<View> {name: ACES 1.3, view_transform: ACES 1.3 Display P3, display_colorspace: %s}
    - !<View> {name: ACES 2.0, view_transform: ACES 2.0 Display P3, display_colorspace: %s}
    - !<View> {name: AgX, view_transform: AgX Base Display P3, display_colorspace: %s}
    - !<View> {name: False Color, view_transform: AgX False Color Rec.709, display_colorspace: %s}
    - !<View> {name: Raw, colorspace: Non-Color}
""" % ((MAC_VIEW,) * 7)

NEW_VIEW_NAMES = ["Standard 1886 Sim"]

CAMERA_REC709_MATRIX = "2.52168618674388, -1.13413098823972, -0.387555198504164, 0, -0.276479914229922, 1.37271908766826, -0.096239173438334, 0, -0.0153780649660342, -0.152975335867399, 1.16835340083343, 0, 0, 0, 0, 1"
CAMERA_REC709 = """
  - !<ColorSpace>
    name: Camera Rec.709
    aliases: [camera_rec709, Utility - Rec.709 - Camera, rec709_camera]
    family: Utility/ITU
    equalitygroup: ""
    bitdepth: 32f
    description: |
      Rec.709 camera OETF, Rec.709 primaries, D65 — scene-referred capture encoding (from ACES 1.3
      Studio; distinct from BT.1886/Gamma 2.4 display encodings).
    isdata: false
    encoding: sdr-video
    from_scene_reference: !<GroupTransform>
      name: AP0 to Camera Rec.709
      children:
        - !<MatrixTransform> {matrix: [%s]}
        - !<ExponentWithLinearTransform> {gamma: 2.22222222222222, offset: 0.099, direction: inverse}
""" % CAMERA_REC709_MATRIX


# Appended from the standards we borrow from (2026-08-29 parity audit): the one OCIO camera builtin
# with no space in Blender/ACES-2.0 configs, and two ACES-studio display spaces (AE 2.4.0-safe:
# ST2084-P3-D65 is used by AE's own bundled ACES 1.3 configs; the 2.2 display is primitives).
REDLOGFILM = """
  - !<ColorSpace>
    name: REDLogFilm REDWideGamutRGB
    aliases: [redlogfilm_rwg, REDLogFilm RWG, Input - RED - REDLogFilm - REDWideGamutRGB]
    family: Input/RED
    equalitygroup: ""
    bitdepth: 32f
    description: |
      RED LogFilm curve with RED Wide Gamut RGB primaries (legacy RED log; OCIO builtin).
    isdata: false
    encoding: log
    to_scene_reference: !<BuiltinTransform> {style: RED_REDLOGFILM-RWG_to_ACES2065-1}
"""
P3_PQ = "P3-D65 PQ"
def p3_pq_display():
    return ('  - !<ColorSpace>\n    name: %s\n    aliases: [st2084_p3d65_display, ST2084-P3-D65 - Display, P3-D65 ST2084]\n'
            '    family: Display\n    equalitygroup: ""\n    bitdepth: 32f\n    description: |\n'
            '      P3-D65 primaries with the ST 2084 (PQ) EOTF — HDR mastering display (ACES studio "ST2084-P3-D65 - Display").\n'
            '    isdata: false\n    encoding: hdr-video\n'
            '    from_display_reference: !<BuiltinTransform> {style: DISPLAY - CIE-XYZ-D65_to_ST2084-P3-D65}\n') % P3_PQ
EXTRA_DISPLAY_VIEWS = """  %s:
    - !<View> {name: Standard, view_transform: Standard, display_colorspace: %s}
    - !<View> {name: Standard 1886 Sim, view_transform: Standard 1886 Sim, display_colorspace: %s}
    - !<View> {name: ACES 1.3, view_transform: ACES 1.3 Rec.1886, display_colorspace: %s}
    - !<View> {name: ACES 2.0, view_transform: ACES 2.0 Rec.1886, display_colorspace: %s}
    - !<View> {name: AgX, view_transform: AgX Base Rec.1886, display_colorspace: %s}
    - !<View> {name: False Color, view_transform: AgX False Color Rec.709, display_colorspace: %s}
    - !<View> {name: Raw, colorspace: Non-Color}
  %s:
    - !<View> {name: Standard, view_transform: Standard, display_colorspace: %s}
    - !<View> {name: ACES 2.0 - HDR 1000 nits, view_transform: ACES 2.0 Rec.2100-PQ - HDR 1000 nits, display_colorspace: %s}
    - !<View> {name: Raw, colorspace: Non-Color}
""" % ((SDR_WORKING,) * 7 + (P3_PQ,) * 3)

# The native OCIO Display Transform's View popup shows only the FIRST display's view list and never
# refreshes (verified 2026-08-25), so the look-views must exist on the SDR displays too.
LOOK_VIEW_DISPLAYS = ["sRGB", "Display P3", "Rec.1886", "Rec.2020"]

def add_look_views(t):
    for disp in LOOK_VIEW_DISPLAYS:
        anchor = "    - !<View> {name: Standard, view_transform: Standard, display_colorspace: %s}\n" % disp
        ins = anchor
        ins += "    - !<View> {name: Standard 1886 Sim, view_transform: Standard 1886 Sim, display_colorspace: %s}\n" % disp
        if anchor not in t:
            sys.exit("look-view anchor missing for display " + disp)
        t = t.replace(anchor, ins, 1)
    return t



VIEW_BAKED_CS = """
  - !<ColorSpace>
    name: %s
    aliases: [UnionMacOS 1886 Sim]
    family: Display
    equalitygroup: ""
    bitdepth: 32f
    description: |
      Standard 1886 Sim view baked for the macOS View Only display (P3 primaries, pure 2.2), self-contained
      primitives only — AE's effect popup builder fails on scene colourspaces that reference display
      colourspaces (see probes/RESULTS.md section 14).
    isdata: false
    encoding: sdr-video
    from_scene_reference: !<GroupTransform>
      children:
        - !<ColorSpaceTransform> {src: ACES2065-1, dst: Linear Rec.709}
        - !<ExponentTransform> {value: 2.4, direction: inverse}
        - !<ExponentWithLinearTransform> {gamma: 2.4, offset: 0.055}
        - !<ColorSpaceTransform> {src: Linear Rec.709, dst: Linear DCI-P3 D65}
        - !<ExponentTransform> {value: 2.2, direction: inverse}

  - !<ColorSpace>
    name: sRGB 1886 Sim
    family: Display
    equalitygroup: ""
    bitdepth: 32f
    description: |
      Standard 1886 Sim view baked for the sRGB display, self-contained primitives only.
    isdata: false
    encoding: sdr-video
    from_scene_reference: !<GroupTransform>
      children:
        - !<ColorSpaceTransform> {src: ACES2065-1, dst: Linear Rec.709}
        - !<ExponentTransform> {value: 2.4, direction: inverse}
        - !<ExponentWithLinearTransform> {gamma: 2.4, offset: 0.055}
        - !<ExponentWithLinearTransform> {gamma: 2.4, offset: 0.055, direction: inverse}
""" % MAC_VIEW_SIM

def sub1(pattern, repl, text, what, flags=re.M):
    new, n = re.subn(pattern, repl, text, count=1, flags=flags)
    if n != 1:
        sys.exit("generate.py: anchor not found: " + what)
    return new


# ---------------------------------------------------------------------------------------------
# block / section surgery. A block ends at the next `  - !<` item, the next top-level key, or EOF —
# NEVER at a blank line (the master contains a whitespace-only line after Linear FilmLight E-Gamut).
BLOCK_END = r"(?=^  - !<|^[a-z_]+:|\Z)"

def block_re(kind, name):
    return re.compile(r"^  - !<%s>\n    name: %s\n.*?%s" % (kind, re.escape(name), BLOCK_END), re.M | re.S)

def get_block(t, kind, name):
    ms = block_re(kind, name).findall(t)
    if len(ms) != 1:
        sys.exit("generate.py: expected exactly one %s block named %r, found %d" % (kind, name, len(ms)))
    return ms[0]

def drop_block(t, kind, name):
    get_block(t, kind, name)
    return block_re(kind, name).sub("", t, count=1)

def section(t, key):
    m = re.search(r"^%s:.*?(?=^[a-z_]+:|\Z)" % key, t, re.M | re.S)
    if not m:
        sys.exit("generate.py: section not found: " + key)
    return m

def replace_section(t, key, body):
    m = section(t, key)
    return t[:m.start()] + body + t[m.end():]

def xyz_matrix_of(t, cs):
    """The XYZ-D65 -> RGB matrix text of a master linear space (verbatim, no numeric round-trip)."""
    blk = get_block(t, "ColorSpace", cs)
    m = re.search(r"MatrixTransform> \{matrix: \[([^\]]+)\]", blk)
    if not m:
        sys.exit("generate.py: no matrix in " + cs)
    return m.group(1)

def aliases_of(t, kind, cs):
    m = re.search(r"^    aliases: (.*)$", get_block(t, kind, cs), re.M)
    return m.group(1) if m else None


def probe_matrix(cfg, src, dst):
    """Composite src->dst as one row-major 4x4 matrix, by probing basis vectors (asserts linear)."""
    p = cfg.getProcessor(src, dst).getDefaultCPUProcessor()
    z = p.applyRGB([0.0, 0.0, 0.0])
    assert max(abs(x) for x in z) < 1e-9, "not affine-free: %s -> %s" % (src, dst)
    cols = [p.applyRGB(v) for v in ([1.0, 0.0, 0.0], [0.0, 1.0, 0.0], [0.0, 0.0, 1.0])]
    rows = [[cols[j][i] for j in range(3)] for i in range(3)]
    lin = p.applyRGB([0.3, 0.5, 0.7])
    ref = [0.3 * rows[i][0] + 0.5 * rows[i][1] + 0.7 * rows[i][2] for i in range(3)]
    assert max(abs(a - b) for a, b in zip(lin, ref)) < 1e-6, "not linear: %s -> %s" % (src, dst)
    return ", ".join("%.10g" % x for r in rows for x in (r + [0.0])) + ", 0, 0, 0, 1"


# ---------------------------------------------------------------------------------------------
SRGB_INV = "!<ExponentWithLinearTransform> {gamma: 2.4, offset: 0.055, direction: inverse}"
G24_INV = "!<ExponentTransform> {value: 2.4, direction: inverse}"
G22_INV = "!<ExponentTransform> {value: 2.2, direction: inverse}"

def self_contained_display(name, matrix, curve, aliases, desc):
    return ("  - !<ColorSpace>\n"
            "    name: %s\n"
            + ("    aliases: %s\n" % aliases if aliases else "") +
            "    family: Display\n"
            "    equalitygroup: \"\"\n"
            "    bitdepth: 32f\n"
            "    description: |\n"
            "      %s\n"
            "      Self-contained (XYZ-D65 matrix + curve): no nested reference crossing, so the SDR\n"
            "      family's default_view_transform bridge applies exactly once.\n"
            "    isdata: false\n"
            "    encoding: sdr-video\n"
            "    from_display_reference: !<GroupTransform>\n"
            "      children:\n"
            "        - !<MatrixTransform> {matrix: [%s]}\n"
            "        - %s\n") % (name, desc, matrix, curve)


def direct_linear(t, name, matrix):
    """Replace `CST(ACES2065-1 -> Linear CIE-XYZ D65) + Matrix` with one direct AP0->RGB matrix."""
    blk = get_block(t, "ColorSpace", name)
    pat = (r"    from_scene_reference: !<GroupTransform>\n      children:\n"
           r"        - !<ColorSpaceTransform> \{src: ACES2065-1, dst: Linear CIE-XYZ (?:I-)?D65\}\n"
           r"        - !<MatrixTransform> \{matrix: \[[^\]]+\]\}\n")
    new, n = re.subn(pat, "    from_scene_reference: !<MatrixTransform> {matrix: [%s]}\n" % matrix, blk, count=1)
    if n != 1:
        sys.exit("generate.py: nested XYZ group not found in " + name)
    return t.replace(blk, new, 1)


def rewrite_vt(blk, name, m709):
    """Make a view transform self-contained: its tail must not cross into the display reference."""
    if name == "AgX Base Rec.1886":
        return sub1(r"^        - !<ColorSpaceTransform> \{src: Rec\.1886, dst: cie_xyz_d65_interchange\}\n",
                    "        - !<ExponentTransform> {value: 2.4}\n"
                    "        - !<MatrixTransform> {matrix: [%s], direction: inverse}\n" % m709, blk, "AgX tail")
    if name == "Filmic sRGB":
        return sub1(r"^        - !<ColorSpaceTransform> \{src: sRGB, dst: cie_xyz_d65_interchange\}\n",
                    "        - !<ExponentWithLinearTransform> {gamma: 2.4, offset: 0.055}\n"
                    "        - !<MatrixTransform> {matrix: [%s], direction: inverse}\n" % m709, blk, "Filmic tail")
    if "ColorSpaceTransform" in blk:
        sys.exit("generate.py: view transform %s has a nested ColorSpaceTransform" % name)
    return blk


SDR_DISPLAYS = ["sRGB", "Rec.1886", "Display P3", "Rec.2020", SDR_WORKING, MAC_VIEW, MAC_DESKTOP, MAC_VIDEO, WIN_DESKTOP, WIN_VIDEO]
SDR_VIEW_ONLY = [MAC_VIEW] + PLATFORM_VIEWS                            # legacy stays valid + view-only; the panel LISTS only the platform views
SDR_VIEW_SPACES = PLATFORM_VIEWS + ["sRGB", "Display P3", "Rec.1886", SDR_WORKING]
SDR_RENDER_SPACES = RENDER_ONLY + ["Rec.1886", "sRGB", "Display P3", "Rec.2020", SDR_WORKING]
SDR_INPUT_PRIORITY = [SDR_WORKING, "sRGB", "Rec.1886", "Display P3", "Rec.2020", "Linear Rec.709",
                      "Linear Rec.2020", "ACEScg", "ACES2065-1", "Non-Color"]
SDR_INACTIVE = ["Linear CIE-XYZ D65", "Linear CIE-XYZ E", "Luminance Compensation Rec.2020", "AgX Log",
                "Filmic Log", "Linear FilmLight E-Gamut", "Camera Rec.709", "Gamma 1.8 Encoded Rec.709",
                "Gamma 2.2 Encoded Rec.709", "Gamma 2.4 Encoded Rec.709", "sRGB Encoded P3-D65",
                "Gamma 2.2 Encoded AdobeRGB", "sRGB Encoded AP1", "Gamma 2.2 Encoded AP1"]
SDR_VTS = ["Standard"]                                             # pure SDR: no tone-mapped view transforms in the config at all
BRIDGE_VTS = ["Standard", "AgX Base Rec.1886", "ACES 2.0 Rec.1886", "ACES 1.3 Rec.1886", "Filmic sRGB"]   # rewrite_vt knows these
LINEAR_DIRECT = ["Linear Rec.709", "Linear DCI-P3 D65", "Linear Rec.2020", "Linear FilmLight E-Gamut"]


def make_sdr(base, flavour, mats):
    """The SDR branch: display-referred working space + rendering bridge, all self-contained."""
    if flavour not in BRIDGE_VTS:
        sys.exit("generate.py: unknown SDR flavour " + flavour)
    vts = SDR_VTS if flavour == "Standard" else ["Standard", flavour]
    t = base
    m709, mp3, m2020 = xyz_matrix_of(base, "Linear Rec.709"), xyz_matrix_of(base, "Linear DCI-P3 D65"), xyz_matrix_of(base, "Linear Rec.2020")
    # displays: Standard + Raw only — a tone-mapped view on display-referred pixels is a double transform
    disp = "displays:\n"
    for d in SDR_DISPLAYS:
        disp += "  %s:\n" % d
        disp += "    - !<View> {name: Standard, view_transform: Standard, display_colorspace: %s}\n" % d
        disp += "    - !<View> {name: Raw, colorspace: Non-Color}\n"
    t = replace_section(t, "displays", disp)
    t = sub1(r"^active_displays: \[.*\]$", "active_displays: [" + ", ".join(SDR_DISPLAYS) + "]", t, "active_displays")
    t = sub1(r"^active_views: \[.*\]$", "active_views: [Standard, Raw]", t, "active_views")
    t = sub1(r"^inactive_colorspaces: \[.*\]$", "inactive_colorspaces: [" + ", ".join(SDR_INACTIVE) + "]", t, "inactive_colorspaces")
    # display colourspaces: six self-contained + the (empty) display reference for the interchange role
    dcs = "display_colorspaces:\n"
    dcs += self_contained_display("sRGB", m709, SRGB_INV, aliases_of(base, "ColorSpace", "sRGB"), "sRGB IEC 61966-2-1 piecewise encoding, Rec.709 primaries") + "\n"
    dcs += self_contained_display("Rec.1886", m709, G24_INV, aliases_of(base, "ColorSpace", "Rec.1886"), "BT.1886 (pure 2.4 EOTF), Rec.709 primaries — broadcast video") + "\n"
    dcs += self_contained_display("Display P3", mp3, SRGB_INV, aliases_of(base, "ColorSpace", "Display P3"), "Apple Display P3: P3-D65 primaries, sRGB piecewise curve") + "\n"
    dcs += self_contained_display("Rec.2020", m2020, G24_INV, aliases_of(base, "ColorSpace", "Rec.2020"), "BT.2020 primaries, pure 2.4 EOTF") + "\n"
    dcs += self_contained_display(SDR_WORKING, m709, G22_INV, "[g22_rec709_display, Gamma 2.2 Rec.709 - Display]",
                                  "Rec.709 primaries, pure 2.2 EOTF — the SDR family's WORKING space (what a desktop display does;\n      a naked render carries these values under a BT.709 tag).") + "\n"
    dcs += self_contained_display(MAC_VIEW, mp3, G22_INV, "[UnionMacOS]",
                                  "P3-D65 primaries, pure 2.2 EOTF — AE's macOS viewport surface (P3-tagged, EDR off). VIEW ONLY (legacy name).") + "\n"
    dcs += mac_display_pair(m709, mp3) + "\n" + platform_pair(m709) + "\n"
    dcs += get_block(base, "ColorSpace", "Linear CIE-XYZ D65")
    if not dcs.endswith("\n\n"):
        dcs += "\n"
    t = replace_section(t, "display_colorspaces", dcs)
    t = sub1(r"^default_view_transform: .*$", "default_view_transform: " + flavour, t, "default_view_transform")
    vtxt = "view_transforms:\n" + "".join(rewrite_vt(get_block(base, "ViewTransform", n), n, m709) for n in vts)
    if not vtxt.endswith("\n\n"):
        vtxt += "\n"
    t = replace_section(t, "view_transforms", vtxt)
    t = drop_block(t, "ColorSpace", MAC_VIEW_SIM)
    t = drop_block(t, "ColorSpace", "sRGB 1886 Sim")
    for cs in LINEAR_DIRECT:
        t = direct_linear(t, cs, mats[cs])
    t = sub1(r"^looks:\n.*\Z", "", t, "looks", flags=re.M | re.S)
    return t


# ---------------------------------------------------------------------------------------------
def load_master_with_patch():
    work = os.path.join(DIST, "_work")
    if os.path.exists(work):
        shutil.rmtree(work)
    os.makedirs(work)
    shutil.copy(os.path.join(MASTER, "config.ocio"), work)
    r = subprocess.run(["patch", "-s", os.path.join(work, "config.ocio"),
                        os.path.join(HERE, "patches", "blender52-edr.patch")],
                       capture_output=True, text=True)
    if r.returncode != 0:
        sys.exit("EDR patch failed:\n" + r.stdout + r.stderr)
    t = open(os.path.join(work, "config.ocio"), encoding="utf-8").read()
    shutil.rmtree(work)
    return t


def mac_display_pair(m709, mp3):
    """The two forward-looking mac views as SELF-CONTAINED display colourspaces (Desktop == legacy
    macOS View Only transform; Video == the 1886 sim on the same surface)."""
    desk = self_contained_display(MAC_DESKTOP, mp3, G22_INV, "[macOS Desktop View Only]",
                                  "P3-D65 primaries, pure 2.2 EOTF — AE's macOS viewport surface (P3-tagged, EDR off). VIEW ONLY.")
    video = ("  - !<ColorSpace>\n    name: %s\n    aliases: [macOS Video View Only]\n    family: Display\n    equalitygroup: \"\"\n    bitdepth: 32f\n"
             "    description: |\n      macOS Desktop View preceded by a BT.1886 (2.4) encode + desktop sRGB decode —\n"
             "      predicts how a Rec.709 video delivery plays through a desktop pipeline (QuickTime-style). VIEW ONLY.\n"
             "    isdata: false\n    encoding: sdr-video\n    from_display_reference: !<GroupTransform>\n      children:\n"
             "        - !<MatrixTransform> {matrix: [%s]}\n"
             "        - !<ExponentTransform> {value: 2.4, direction: inverse}\n"
             "        - !<ExponentWithLinearTransform> {gamma: 2.4, offset: 0.055}\n"
             "        - !<MatrixTransform> {matrix: [%s], direction: inverse}\n"
             "        - !<MatrixTransform> {matrix: [%s]}\n"
             "        - !<ExponentTransform> {value: 2.2, direction: inverse}\n") % (MAC_VIDEO, m709, m709, mp3)
    return desk + "\n" + video

def platform_pair(m709):
    """Windows views + the two render targets: plain duplicates of sRGB / Rec.1886, self-contained,
    named for what the user is doing rather than for a standard."""
    return (self_contained_display(WIN_DESKTOP, m709, SRGB_INV, "[Windows Desktop View Only]", "sRGB — a Windows desktop surface. View target only.") + "\n" +
            self_contained_display(WIN_VIDEO, m709, G24_INV, "[Windows Video View Only]", "BT.1886 (2.4) — a Rec.709 video delivery on a 2.4 display. View target only.") + "\n" +
            self_contained_display(DESKTOP_RENDER, m709, SRGB_INV, None, "sRGB values — render target for desktop / web deliveries.") + "\n" +
            self_contained_display(VIDEO_RENDER, m709, G24_INV, None, "BT.1886 (2.4) values — render target for video deliveries."))

MAC_NEW_VIEWS = """  %s:
    - !<View> {name: Standard, view_transform: Standard, display_colorspace: %s}
    - !<View> {name: Raw, colorspace: Non-Color}
  %s:
    - !<View> {name: Standard, view_transform: Standard, display_colorspace: %s}
    - !<View> {name: Raw, colorspace: Non-Color}
  %s:
    - !<View> {name: Standard, view_transform: Standard, display_colorspace: %s}
    - !<View> {name: Raw, colorspace: Non-Color}
  %s:
    - !<View> {name: Standard, view_transform: Standard, display_colorspace: %s}
    - !<View> {name: Raw, colorspace: Non-Color}
""" % (MAC_DESKTOP, MAC_DESKTOP, MAC_VIDEO, MAC_VIDEO, WIN_DESKTOP, WIN_DESKTOP, WIN_VIDEO, WIN_VIDEO)

def add_union(t):
    # 1886-sim as a ViewTransform (looks are invisible to the native Display Transform effect)
    t = sub1(r"^view_transforms:\n", "view_transforms:\n" + UNION_VIEW_TRANSFORM, t, "view_transforms:")
    # display colourspaces at the top of display_colorspaces: legacy macOS View Only + the Desktop/Video pair
    m709, mp3 = xyz_matrix_of(t, "Linear Rec.709"), xyz_matrix_of(t, "Linear DCI-P3 D65")
    g22 = self_contained_display(SDR_WORKING, m709, G22_INV, "[g22_rec709_display, Gamma 2.2 Rec.709 - Display]",
                                 "Rec.709 primaries, pure 2.2 EOTF (ACES studio \"Gamma 2.2 Rec.709 - Display\"; the SDR family's working space).")
    t = sub1(r"^display_colorspaces:\n", "display_colorspaces:\n" + MAC_DISPLAY_CS.lstrip("\n") + "\n" + mac_display_pair(m709, mp3) + "\n" + platform_pair(m709) + "\n" + g22 + "\n" + p3_pq_display() + "\n", t, "display_colorspaces:")
    # camera spaces at the TOP of the colorspaces section (the master file ends with looks:, not colorspaces:)
    t = sub1(r"^colorspaces:\n", "colorspaces:\n" + CAMERA_REC709.strip("\n") + "\n\n" + REDLOGFILM.strip("\n") + "\n\n" + VIEW_BAKED_CS.strip("\n") + "\n\n", t, "colorspaces anchor")
    # views block just before active_displays
    t = sub1(r"^active_displays:", MAC_VIEWS + MAC_NEW_VIEWS + EXTRA_DISPLAY_VIEWS + "active_displays:", t, "active_displays anchor")
    t = add_look_views(t)
    # activate display + views
    t = sub1(r"^active_displays: \[(.*)\]$", lambda m: "active_displays: [" + m.group(1) + ", " + MAC_VIEW + ", " + MAC_DESKTOP + ", " + MAC_VIDEO + ", " + WIN_DESKTOP + ", " + WIN_VIDEO + ", " + SDR_WORKING + ", " + P3_PQ + "]", t, "active_displays list")
    t = sub1(r"^active_views: \[(.*)\]$", lambda m: "active_views: [" + m.group(1) + ", " + ", ".join(NEW_VIEW_NAMES) + "]", t, "active_views list")
    return t


def add_file_rules(t, default_cs):
    # SINGLE-AUTHORITY MODEL (PREPLAN 3.6b final): no bucket rules — imports are never interpreted.
    # The Default rule == working space is the entire contract.
    rules = "file_rules:\n  - !<Rule> {name: Default, colorspace: %s}\n\n" % default_cs
    return sub1(r"^roles:", rules + "roles:", t, "roles anchor")


def set_roles(t, working, linear):
    t = sub1(r"^  default: Linear Rec\.709$", "  default: " + working, t, "default role")
    if linear:
        t = sub1(r"^  scene_linear: Linear Rec\.709$", "  scene_linear: " + working, t, "scene_linear role")
        t = sub1(r"^  rendering: Linear Rec\.709$", "  rendering: " + working, t, "rendering role")
    return t


def downlevel_24(t):
    t = sub1(r"^ocio_profile_version: .*$", "ocio_profile_version: 2.4", t, "version")
    t = re.sub(r"^[ \t]*interop_id:.*\n", "", t, flags=re.M)
    t = re.sub(r"^[ \t]*interchange:[ \t]*\n(?:[ \t]+icc_profile_name:.*\n)+", "", t, flags=re.M)
    return t


def pwcs_json(name, family):
    data = base64.b64encode(json.dumps({"colorSpace1": name}, separators=(",", ":")).encode()).decode()
    return json.dumps({"baseColorProfile": {"colorProfileData": data, "colorProfileName": family + "/" + name},
                       "baseProfileType": 3}, separators=(",", ":"))


# ---------------------------------------------------------------------------------------------
def validate(path, working, expect_default, view_checks):
    r = subprocess.run(["ociocheck", "-iconfig", path], capture_output=True, text=True)
    outerr = r.stdout + r.stderr
    if "Tests complete" not in outerr:
        sys.exit("ociocheck failed for %s:\n%s" % (path, outerr[-1200:]))
    import PyOpenColorIO as ocio
    cfg = ocio.Config.CreateFromFile(path)
    cfg.validate()
    assert cfg.getColorSpace(working), "working space missing: " + working
    # single-authority: every path must resolve to the Default rule (no interpretation at import)
    for probe in ["/jobs/x/shot/file.exr", "C:\\jobs\\x\\file.mov", "/anything/bucket_srgb/file.png"]:
        assert cfg.getColorSpaceFromFilepath(probe)[0] == expect_default, "unexpected file-rule interpretation for " + probe
    for display, view in view_checks:
        ocio_t = ocio.DisplayViewTransform(src=working, display=display, view=view)
        cfg.getProcessor(ocio_t).getDefaultCPUProcessor()
    return cfg


LINEAR_VIEW_CHECKS = [(MAC_VIEW, v) for v in ["Standard", "Standard 1886 Sim", "ACES 2.0", "AgX"]] + [(d, "Standard") for d in PLATFORM_VIEWS] + [(SDR_WORKING, "AgX"), (P3_PQ, "ACES 2.0 - HDR 1000 nits")]
SDR_VIEW_CHECKS = [(d, v) for d in SDR_DISPLAYS for v in ["Standard", "Raw"]]

# genuine master views per flavour: (display, view, decode) — used for cross-config fidelity
FLAVOUR_REF = {
    "Standard":          ("Rec.1886", "Standard", "g24"),
    "AgX Base Rec.1886": ("Rec.1886", "AgX", "g24"),
    "ACES 2.0 Rec.1886": ("Rec.1886", "ACES 2.0", "g24"),
    "ACES 1.3 Rec.1886": ("Rec.1886", "ACES 1.3", "g24"),
    "Filmic sRGB":       ("sRGB", "Filmic", "srgb"),
}

def _srgb_dec(v):
    v = max(v, 0.0)
    return v / 12.92 if v <= 0.04045 else ((v + 0.055) / 1.055) ** 2.4
def _srgb_enc(l):
    l = max(l, 0.0)
    return 12.92 * l if l <= 0.0031308 else 1.055 * l ** (1 / 2.4) - 0.055
def _g(v, p):
    return max(v, 0.0) ** p


def validate_sdr(path, flavour, master_path):
    """Runs in a SUBPROCESS: proves the bridge on the shipped SDR config (a regression segfaults)."""
    import PyOpenColorIO as ocio
    text = open(path, encoding="utf-8").read()
    # (b) text asserts: no nested reference crossing survives, no looks / sims / HDR displays
    disp_names = re.findall(r"^    name: (.*)$", section(text, "display_colorspaces").group(0), re.M)
    forbidden = ["cie_xyz_d65_interchange", "Linear CIE-XYZ"] + disp_names
    for m in re.finditer(r"!<ColorSpaceTransform> \{src: ([^,]+), dst: ([^,}]+)", text):
        for end in (m.group(1).strip(), m.group(2).strip()):
            for f in forbidden:
                assert not end.startswith(f), "nested reference crossing survived: %s (%s)" % (m.group(0), f)
    assert "\nlooks:" not in text and "1886 Sim" not in text and "Rec.2100" not in text, "SDR config carries dropped content"
    assert re.search(r"^default_view_transform: %s$" % re.escape(flavour), text, re.M), "bridge not set"
    # (a)
    cfg = validate(path, SDR_WORKING, SDR_WORKING, SDR_VIEW_CHECKS)
    W = SDR_WORKING
    def cst(c, s, d, v):
        return c.getProcessor(s, d).getDefaultCPUProcessor().applyRGB(list(v))
    # (c) bridge exactness: the CST into the working space == a genuine DisplayViewTransform through the flavour VT
    ref = ocio.Config.CreateFromFile(path)
    ref.setActiveViews("")
    ref.addDisplayView(W, "_bridge_ref", flavour, W, "")
    dvt = lambda s, v: ref.getProcessor(ocio.DisplayViewTransform(src=s, display=W, view="_bridge_ref")).getDefaultCPUProcessor().applyRGB(list(v))
    LIN = [(0.18,) * 3, (1.0,) * 3, (0.0, 1.0, 0.0), (0.5, 0.1, 0.02), (4.0,) * 3, (0.01, 0.02, 0.03)]
    LOG = [(0.278,) * 3, (0.5, 0.4, 0.3), (0.9, 0.9, 0.9), (0.1, 0.2, 0.15)]
    worst = 0.0
    for src, pts in [("Linear Rec.709", LIN), ("Linear Rec.2020", LIN), ("ACEScg", LIN), ("ARRI LogC4", LOG), ("S-Log3 S-Gamut3", LOG)]:
        for v in pts:
            a, b = cst(cfg, src, W, v), dvt(src, v)
            worst = max(worst, max(abs(x - y) for x, y in zip(a, b)))
    assert worst < 1e-4, "bridge CST != DisplayViewTransform (worst %.2e)" % worst
    four = cst(cfg, "Linear Rec.709", W, (4.0,) * 3)[0]
    if flavour == "Standard":                                        # pure SDR: linear 4.0 must be the plain 2.2 encode (no tone map, no clamp)
        assert abs(four - 4.0 ** (1 / 2.2)) < 1e-3, "Standard bridge is not pure (4.0 -> %.4f)" % four
    else:                                                            # guard against silent cancellation: a bridged 4.0 must be tone-mapped, not 1.88
        assert four < 1.2, "bridge cancelled (4.0 not tone-mapped)"
    # (d) cross-config fidelity: the rewritten VT == the master's original view, re-encoded to 2.2
    mcfg = ocio.Config.CreateFromFile(master_path)
    rd, rv, dec = FLAVOUR_REF[flavour]
    worst = 0.0
    for src, pts in [("Linear Rec.709", LIN), ("ARRI LogC4", LOG)]:
        for v in pts:
            g = mcfg.getProcessor(ocio.DisplayViewTransform(src=src, display=rd, view=rv)).getDefaultCPUProcessor().applyRGB(list(v))
            exp = [_g(_srgb_dec(x) if dec == "srgb" else _g(x, 2.4), 1 / 2.2) for x in g]
            got = cst(cfg, src, W, v)
            worst = max(worst, max(abs(x - y) for x, y in zip(exp, got)))
    assert worst < 2e-4, "rewritten view transform drifted from the master's view (worst %.2e)" % worst
    # (e) display purity: display->display never touches the bridge
    for v in [(0.02,) * 3, (0.18,) * 3, (0.5,) * 3, (0.9,) * 3, (1.0, 0.0, 0.0)]:
        got = cst(cfg, "sRGB", W, v); exp = [_g(_srgb_dec(x), 1 / 2.2) for x in v]
        assert max(abs(a - b) for a, b in zip(got, exp)) < 1e-4, "sRGB -> working not pure"
        got = cst(cfg, "Rec.1886", W, v); exp = [_g(_g(x, 2.4), 1 / 2.2) for x in v]
        assert max(abs(a - b) for a, b in zip(got, exp)) < 1e-4, "Rec.1886 -> working not pure"
        got = cst(cfg, W, "Rec.1886", v); exp = [_g(_g(x, 2.2), 1 / 2.4) for x in v]
        assert max(abs(a - b) for a, b in zip(got, exp)) < 1e-4, "working -> Rec.1886 not pure"
        got = cst(cfg, W, W, v)
        assert max(abs(a - b) for a, b in zip(got, v)) < 1e-5, "working -> working not identity"
    got = cst(cfg, "Display P3", W, (1.0,) * 3)
    assert max(abs(a - 1.0) for a in got) < 1e-4, "Display P3 white != working white"
    got = cst(cfg, W, MAC_VIEW, (0.5,) * 3)
    assert max(abs(a - 0.5) for a in got) < 1e-4, "working -> macOS View Only grey drifted (should be matrix-only)"
    a, b = cst(cfg, W, MAC_VIEW, (1.0, 0.0, 0.0)), cst(cfg, W, MAC_DESKTOP, (1.0, 0.0, 0.0))
    assert max(abs(x - y) for x, y in zip(a, b)) < 1e-6, "Desktop view != legacy macOS View Only"
    got = cst(cfg, W, MAC_VIDEO, (0.5,) * 3); exp = _g(_srgb_dec(_g(_g(0.5, 2.2), 1 / 2.4)), 1 / 2.2)
    assert max(abs(x - exp) for x in got) < 1e-4, "Video view != sim math (%.4f vs %.4f)" % (got[0], exp)
    for dup, ref in [(WIN_DESKTOP, "sRGB"), (DESKTOP_RENDER, "sRGB"), (WIN_VIDEO, "Rec.1886"), (VIDEO_RENDER, "Rec.1886")]:
        for v in [(0.05,) * 3, (0.5,) * 3, (1.0, 0.0, 0.0)]:
            a, b = cst(cfg, W, dup, v), cst(cfg, W, ref, v)
            assert max(abs(x - y) for x, y in zip(a, b)) < 1e-6, "%s != %s" % (dup, ref)
    # (f) the inverse path (display -> scene through the inverse VT) must at least build
    cfg.getProcessor(W, "Linear Rec.709").getDefaultCPUProcessor()
    print("  validate_sdr ok  %s  bridge=%s" % (os.path.basename(path), flavour))


def per_preset_menus(cfg, p):
    import PyOpenColorIO as ocio
    names = list(cfg.getColorSpaceNames())          # ACTIVE colourspaces only
    def hoist(lst, pri):
        return [v for v in pri if v in lst] + sorted(v for v in lst if v not in pri)
    if p["bridge"]:
        view_only = list(SDR_VIEW_ONLY)
        inputs = []
        for n in names:
            cs = cfg.getColorSpace(n)
            fam = cs.getFamily() or ""
            if fam.startswith("Display") and (n in view_only or n in RENDER_ONLY):
                continue
            inputs.append(n)
        inputs = hoist(inputs, SDR_INPUT_PRIORITY)
        views, renders = list(SDR_VIEW_SPACES), list(SDR_RENDER_SPACES)
    else:
        input_spaces, view_spaces = [], []
        for n in names:
            cs = cfg.getColorSpace(n)
            fam = cs.getFamily() or ""
            if cs.isData():
                input_spaces.append(n)                               # Non-Color is a legit footage assignment
            elif fam.startswith("View Inverse"):
                view_spaces.append(n)                                # tone-map inverses: view-only
            elif fam.startswith("Display"):
                if n in RENDER_ONLY:
                    continue                                         # render targets: neither view nor input
                view_spaces.append(n)
                if "1886 Sim" not in n and n != MAC_VIEW and n not in PLATFORM_VIEWS:
                    input_spaces.append(n)                           # sRGB/Rec.1886/P3/... are also footage encodings
            else:
                input_spaces.append(n)
        view_only = [MAC_VIEW] + PLATFORM_VIEWS + [MAC_VIEW_SIM, "sRGB 1886 Sim"]
        view_spaces = [v for v in view_spaces if v not in (MAC_VIEW, MAC_VIEW_SIM)]   # legacy names stay in the config, not in the menu
        views = hoist(view_spaces, PLATFORM_VIEWS + ["sRGB", "Rec.1886", "Display P3", "sRGB 1886 Sim"])
        inputs = hoist(input_spaces, ["sRGB", "Rec.1886", "Gamma 2.4 Encoded Rec.709", "Display P3", "Camera Rec.709",
                                      "Linear Rec.709", "Linear Rec.2020", "ACEScg", "ACES2065-1"])
        seen, renders = set(), []
        for n in RENDER_ONLY + views + inputs:
            if n in seen or n in view_only:
                continue
            seen.add(n); renders.append(n)
    for lst in (inputs, views, renders, view_only):
        for n in lst:
            assert cfg.getColorSpace(n), "menu names a colourspace missing from the config: " + n
    return {"inputSpaces": inputs, "viewSpaces": views, "renderSpaces": renders, "viewOnly": view_only}


def main():
    for k in list(PRESETS) + list(RETIRED):
        assert re.match(r"^[A-Za-z0-9]+$", k), "preset key must be alphanumeric (panel CONFIG_PATTERN): " + k
    os.makedirs(DIST, exist_ok=True)
    for d in ("luts", "filmic", "icc"):
        dst = os.path.join(DIST, d)
        if not os.path.exists(dst):
            shutil.copytree(os.path.join(MASTER, d), dst)
    base = add_union(load_master_with_patch())

    # 2.5 master (QCView / tooling): neutral AP0 default, roles untouched
    mpath = os.path.join(DIST, "config-master-2.5.ocio")
    open(mpath, "w", encoding="utf-8").write(add_file_rules(base, "ACES2065-1"))
    import PyOpenColorIO as ocio
    base_cfg = ocio.Config.CreateFromFile(mpath)
    mats = {cs: probe_matrix(base_cfg, "ACES2065-1", cs) for cs in LINEAR_DIRECT}
    ref709 = [float(x) for x in CAMERA_REC709_MATRIX.split(",")]
    got709 = [float(x) for x in mats["Linear Rec.709"].split(",")]
    assert max(abs(a - b) for a, b in zip(ref709, got709)) < 1e-5, "probed AP0->Rec.709 matrix disagrees with CAMERA_REC709 (float32 probe; expect ~1e-7)"

    presets = {}
    for key, p in PRESETS.items():
        linear = p["bridge"] is None
        t = base if linear else make_sdr(base, p["bridge"], mats)
        t = add_file_rules(t, p["working"])
        t = set_roles(t, p["working"], linear)
        t = downlevel_24(t)
        h = hashlib.sha1(t.encode()).hexdigest()[:8]
        # AE caches parsed configs per path per session (verified 2026-08-25): a changed config MUST
        # get a new path, or running sessions keep serving the stale parse. Hence content-hashed names.
        path = os.path.join(DIST, "config-%s-%s.ocio" % (key, h))
        # NEVER delete old hashed configs: projects and the sticky pref keep absolute paths, and a
        # project opened with an unreachable config enters a broken, non-recoverable OCIO state for
        # the session (Adobe bug; verified 2026-08-26). Old versions stay (they are ~70KB each).
        open(path, "w", encoding="utf-8").write(t)
        cfg = validate(path, p["working"], p["working"], LINEAR_VIEW_CHECKS if linear else SDR_VIEW_CHECKS)
        if not linear:
            r = subprocess.run([sys.executable, os.path.abspath(__file__), "--validate-sdr", path, p["bridge"], mpath],
                               capture_output=True, text=True)
            if r.returncode < 0 or r.returncode > 128:
                sys.exit("validate_sdr CRASHED (signal %d) for %s — a nested display/XYZ reference regression\n%s" % (r.returncode, path, r.stderr[-1500:]))
            if r.returncode != 0:
                sys.exit("validate_sdr failed for %s:\n%s%s" % (path, r.stdout[-800:], r.stderr[-1500:]))
            sys.stdout.write(r.stdout)
        rec = {"config": os.path.basename(path), "working": p["working"], "family": p["family"], "label": p["label"],
               "bridge": p["bridge"], "workingSpaceLabel": p["family"] + "/" + p["working"],
               "pwcsJSON": pwcs_json(p["working"], p["family"])}
        rec.update(per_preset_menus(cfg, p))
        presets[key] = rec
        print("  preset %-10s ok  working=%s  %s" % (key, p["working"], "bridge=" + p["bridge"] if p["bridge"] else ""))
    validate(mpath, "ACEScg", "ACES2065-1", LINEAR_VIEW_CHECKS)
    print("  master 2.5 ok")
    retired = {}
    for key, r in RETIRED.items():
        assert os.path.exists(os.path.join(DIST, r["config"])), "retired config missing from the store: " + r["config"]
        retired[key] = dict(r, pwcsJSON=pwcs_json(r["working"], r["family"]))
    lin = presets["acescg"]
    json.dump({"generated": "by config/generate.py", "presets": presets, "retired": retired,
               # top-level lists = the linear family's, for panels that predate per-preset menus
               "inputSpaces": lin["inputSpaces"], "viewSpaces": lin["viewSpaces"], "renderSpaces": lin["renderSpaces"], "viewOnly": lin["viewOnly"]},
              open(os.path.join(DIST, "presets.json"), "w"), indent=1)
    print("dist/ complete")


if __name__ == "__main__":
    if len(sys.argv) >= 5 and sys.argv[1] == "--validate-sdr":
        validate_sdr(sys.argv[2], sys.argv[3], sys.argv[4])
    else:
        main()
