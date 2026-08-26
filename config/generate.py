#!/usr/bin/env python3
"""minColor config generator.

master/ (vendored Blender 5.2 colormanagement, OCIO 2.5)
  + QCView EDR displays (patches/blender52-edr.patch)
  + Union macOS display (P3 primaries, pure 2.2) with the 1886-sim as a ViewTransform
  + file_rules from buckets.json (+ per-preset Default)
  + per-preset roles (identity import: Default rule == working == `default` role)
  + OCIO 2.4 down-level for AE (version tag, interop_id, interchange blocks)

dist/
  luts/ filmic/ icc/                    (shared, copied once)
  config-master-2.5.ocio                (QCView / Blender / tooling)
  config-<preset>.ocio                  (AE; presets below)
  presets.json                          (working name/family + sticky-pref PwCs JSON per preset)

Validation: ociocheck + PyOpenColorIO load/validate + file-rule resolution (both path separators)
+ DisplayViewTransform build for every UnionMacOS view (proves looks resolve).
NOTE: a 2.5 lib cannot prove 2.4 compliance — AE's `ocioConfigurationFile` setter is the final judge.
"""
import base64
import json
import os
import re
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
MASTER = os.path.join(HERE, "master")
DIST = os.path.join(HERE, "dist")

PRESETS = {
    # preset key -> (working colourspace, family shown by AE, linear?)
    "acescg":  ("ACEScg", "Linear", True),
    "ap0":     ("ACES2065-1", "Linear", True),
    "lin709":  ("Linear Rec.709", "Linear", True),
    "lin2020": ("Linear Rec.2020", "Linear", True),
    "sdr":     ("sRGB", "Display", False),   # gamma-encoded compositing; switch to "Gamma 2.4 Encoded Rec.709" for BT.1886 semantics
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

UNION_DISPLAY_CS = """
  - !<ColorSpace>
    name: UnionMacOS
    family: Display
    equalitygroup: ""
    bitdepth: 32f
    description: |
      Display P3 primaries with pure 2.2 EOTF — matches Apple's effective display pipeline when the
      viewport surface is P3-tagged with EDR off (Union AE macOS viewport fix; see docs).
    isdata: false
    encoding: sdr-video
    from_display_reference: !<GroupTransform>
      children:
        - !<ColorSpaceTransform> {src: cie_xyz_d65_interchange, dst: Linear DCI-P3 D65}
        - !<ExponentTransform> {value: 2.2, direction: inverse}
"""

UNION_VIEWS = """  UnionMacOS:
    - !<View> {name: Standard, view_transform: Standard, display_colorspace: UnionMacOS}
    - !<View> {name: Standard 1886 Sim, view_transform: Standard 1886 Sim, display_colorspace: UnionMacOS}
    - !<View> {name: ACES 1.3, view_transform: ACES 1.3 Display P3, display_colorspace: UnionMacOS}
    - !<View> {name: ACES 2.0, view_transform: ACES 2.0 Display P3, display_colorspace: UnionMacOS}
    - !<View> {name: AgX, view_transform: AgX Base Display P3, display_colorspace: UnionMacOS}
    - !<View> {name: False Color, view_transform: AgX False Color Rec.709, display_colorspace: UnionMacOS}
    - !<View> {name: Raw, colorspace: Non-Color}
"""

NEW_VIEW_NAMES = ["Standard 1886 Sim"]

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
        - !<MatrixTransform> {matrix: [2.52168618674388, -1.13413098823972, -0.387555198504164, 0, -0.276479914229922, 1.37271908766826, -0.096239173438334, 0, -0.0153780649660342, -0.152975335867399, 1.16835340083343, 0, 0, 0, 0, 1]}
        - !<ExponentWithLinearTransform> {gamma: 2.22222222222222, offset: 0.099, direction: inverse}
"""


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
    name: UnionMacOS 1886 Sim
    family: Display
    equalitygroup: ""
    bitdepth: 32f
    description: |
      Standard 1886 Sim view baked for the UnionMacOS display (P3 primaries, pure 2.2), self-contained
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
"""

def sub1(pattern, repl, text, what, flags=re.M):
    new, n = re.subn(pattern, repl, text, count=1, flags=flags)
    if n != 1:
        sys.exit("generate.py: anchor not found: " + what)
    return new


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


def add_union(t):
    # 1886-sim as a ViewTransform (looks are invisible to the native Display Transform effect)
    t = sub1(r"^view_transforms:\n", "view_transforms:\n" + UNION_VIEW_TRANSFORM, t, "view_transforms:")
    # display colourspace at the top of display_colorspaces
    t = sub1(r"^display_colorspaces:\n", "display_colorspaces:\n" + UNION_DISPLAY_CS.lstrip("\n") + "\n", t, "display_colorspaces:")
    # camera space at the TOP of the colorspaces section (the master file ends with looks:, not colorspaces:)
    t = sub1(r"^colorspaces:\n", "colorspaces:\n" + CAMERA_REC709.strip("\n") + "\n\n" + VIEW_BAKED_CS.strip("\n") + "\n\n", t, "colorspaces anchor")
    # views block just before active_displays
    t = sub1(r"^active_displays:", UNION_VIEWS + "active_displays:", t, "active_displays anchor")
    t = add_look_views(t)
    # activate display + views
    t = sub1(r"^active_displays: \[(.*)\]$", lambda m: "active_displays: [" + m.group(1) + ", UnionMacOS]", t, "active_displays list")
    t = sub1(r"^active_views: \[(.*)\]$", lambda m: "active_views: [" + m.group(1) + ", " + ", ".join(NEW_VIEW_NAMES) + "]", t, "active_views list")
    return t


def add_file_rules(t, buckets, default_cs):
    # SINGLE-AUTHORITY MODEL (PREPLAN 3.6b final): no bucket rules — imports are never interpreted.
    # The Default rule == working space is the entire contract; `buckets` is ignored here on purpose.
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


def validate(path, working, buckets, expect_default=None):
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
        assert cfg.getColorSpaceFromFilepath(probe)[0] == (expect_default or cfg.getCanonicalName("default")), "unexpected file-rule interpretation for " + probe
    # Union views incl. looks must build
    for view in ["Standard", "Standard 1886 Sim", "ACES 2.0", "AgX"]:
        ocio_t = ocio.DisplayViewTransform(src=working, display="UnionMacOS", view=view)
        cfg.getProcessor(ocio_t).getDefaultCPUProcessor()
    return cfg


def main():
    buckets = json.load(open(os.path.join(HERE, "buckets.json")))["buckets"]
    os.makedirs(DIST, exist_ok=True)
    for d in ("luts", "filmic", "icc"):
        dst = os.path.join(DIST, d)
        if not os.path.exists(dst):
            shutil.copytree(os.path.join(MASTER, d), dst)
    base = add_union(load_master_with_patch())

    # 2.5 master (QCView / tooling): neutral AP0 default, roles untouched
    master_out = add_file_rules(base, buckets, "ACES2065-1")
    mpath = os.path.join(DIST, "config-master-2.5.ocio")
    open(mpath, "w", encoding="utf-8").write(master_out)

    presets = {}
    for key, (working, family, linear) in PRESETS.items():
        t = add_file_rules(base, buckets, working)
        t = set_roles(t, working, linear)
        t = downlevel_24(t)
        import hashlib
        h = hashlib.sha1(t.encode()).hexdigest()[:8]
        # AE caches parsed configs per path per session (verified 2026-08-25): a changed config MUST
        # get a new path, or running sessions keep serving the stale parse. Hence content-hashed names.
        path = os.path.join(DIST, "config-%s-%s.ocio" % (key, h))
        # NEVER delete old hashed configs: projects and the sticky pref keep absolute paths, and a
        # project opened with an unreachable config enters a broken, non-recoverable OCIO state for
        # the session (Adobe bug; verified 2026-08-26). Old versions stay (they are ~70KB each).
        open(path, "w", encoding="utf-8").write(t)
        validate(path, working, buckets, expect_default=working)
        presets[key] = {"config": os.path.basename(path), "working": working, "family": family,
                        "workingSpaceLabel": family + "/" + working, "pwcsJSON": pwcs_json(working, family)}
        print("  preset %-8s ok  working=%s" % (key, working))
    validate(mpath, "ACEScg", buckets, expect_default="ACES2065-1")
    print("  master 2.5 ok")
    # menu lists for the panel (shared across presets — same colourspace set everywhere)
    import PyOpenColorIO as ocio
    any_cfg = ocio.Config.CreateFromFile(os.path.join(DIST, presets["acescg"]["config"]))
    input_spaces, view_spaces = [], []
    for n in any_cfg.getColorSpaceNames():
        cs = any_cfg.getColorSpace(n)
        fam = cs.getFamily() or ""
        if cs.isData():
            input_spaces.append(n)                                   # Non-Color is a legit footage assignment
        elif fam.startswith("View Inverse"):
            view_spaces.append(n)                                    # tone-map inverses: view-only
        elif fam.startswith("Display"):
            view_spaces.append(n)
            if "1886 Sim" not in n:
                input_spaces.append(n)                               # sRGB/Rec.1886/P3/... are also footage encodings (PSDs, video)
        else:
            input_spaces.append(n)
    # curated ordering: common first
    def hoist(lst, names):
        for nm in reversed(names):
            if nm in lst: lst.remove(nm); lst.insert(0, nm)
        return lst
    input_spaces = hoist(sorted(input_spaces), ["sRGB", "Gamma 2.4 Encoded Rec.709", "Camera Rec.709",
        "Linear Rec.709", "ACEScg", "ACES2065-1", "Non-Color"])
    view_spaces = hoist(sorted(view_spaces), ["ACES 2.0 sRGB", "AgX Base sRGB", "UnionMacOS 1886 Sim",
        "sRGB 1886 Sim", "UnionMacOS", "sRGB"])
    # keep the 1886 sims out of the input list (they are views), and displays out of inputs
    for nm in ["sRGB 1886 Sim", "UnionMacOS 1886 Sim"]:
        if nm in input_spaces: input_spaces.remove(nm)
        if nm not in view_spaces: view_spaces.append(nm)
    # curated dropdown orders (audit 2026-08-26): everyday picks first, alphabetical rest
    _priority = ["sRGB", "Rec.1886", "Display P3", "UnionMacOS", "sRGB 1886 Sim", "UnionMacOS 1886 Sim"]
    view_spaces = [v for v in _priority if v in view_spaces] + sorted(v for v in view_spaces if v not in _priority)
    _in_priority = ["sRGB", "Rec.1886", "Gamma 2.4 Encoded Rec.709", "Display P3", "Camera Rec.709",
                    "Linear Rec.709", "Linear Rec.2020", "ACEScg", "ACES2065-1"]
    input_spaces = [v for v in _in_priority if v in input_spaces] + sorted(v for v in input_spaces if v not in _in_priority)
    json.dump({"generated": "by config/generate.py", "presets": presets,
               "inputSpaces": input_spaces, "viewSpaces": view_spaces},
              open(os.path.join(DIST, "presets.json"), "w"), indent=1)
    print("dist/ complete")


if __name__ == "__main__":
    main()
