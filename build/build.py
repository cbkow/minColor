#!/usr/bin/env python3
"""Build the distributable: single-file panel + payload staging.

  dist-panel/minColor.jsx      panel with both .jsxinc files inlined (no #include)
  dist-panel/payload/          configs (hashed, + luts/filmic/icc), presets.json,
                               settings/extension-defaults.json
Run install.command afterwards (or copy by hand).
"""
import os, re, shutil, zipfile
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
SRC = os.path.join(ROOT, "src")
DIST = os.path.join(ROOT, "config", "dist")
OUT = os.path.join(ROOT, "dist-panel")

def inline(panel_text):
    def repl(m):
        inc = open(os.path.join(SRC, m.group(1)), encoding="utf-8").read()
        return "// ==== inlined: %s ====\n%s\n" % (m.group(1), inc)
    return re.sub(r'#include\s+"([^"]+)"\s*\n', repl, panel_text)

def main():
    if os.path.exists(OUT):
        shutil.rmtree(OUT)
    os.makedirs(OUT)
    panel = open(os.path.join(SRC, "minColor Panel.jsx"), encoding="utf-8").read()
    open(os.path.join(OUT, "minColor.jsx"), "w", encoding="utf-8").write(inline(panel))
    open(os.path.join(OUT, "README.txt"), "w", encoding="utf-8").write(
        "minColor — install\n"
        "version: " + re.search(r'var VERSION = \"([^\"]+)\"', open(os.path.join(SRC, "minColor.jsxinc"), encoding="utf-8").read()).group(1) + "\n"
        "requires: After Effects 2025 or later\n\n"
        "Copy BOTH items into your After Effects ScriptUI Panels folder:\n\n"
        "    minColor.jsx\n    minColor-data/\n\n"
        "macOS:   ~/Library/Preferences/Adobe/After Effects/<version>/Scripts/ScriptUI Panels/\n"
        "         (or the app's Scripts/ScriptUI Panels folder)\n"
        "Windows: %APPDATA%\\Adobe\\After Effects\\<version>\\Scripts\\ScriptUI Panels\\\n"
        "         (or Program Files\\Adobe\\...\\Support Files\\Scripts\\ScriptUI Panels\\, needs admin)\n\n"
        "Restart After Effects; the panel appears under Window > minColor.jsx.\n\n"
        "OPTIONAL - the minColor plugin engine (macOS, Apple silicon):\n"
        "Copy plugin-macOS/minColorCST.plugin into:\n"
        "    /Library/Application Support/Adobe/Common/Plug-ins/7.0/MediaCore/minColor/\n"
        "(create the minColor folder if needed). Recommended: also copy the contents of\n"
        "minColor-data/configs into that same minColor folder as 'configs' so projects pin\n"
        "the machine-wide store; without it, projects fall back to per-project sidecars.\n"
        "Windows plugin: not yet built.\n")
    plugin_src = os.path.join(ROOT, "plugin", "build", "minColorCST.plugin")
    if os.path.isdir(plugin_src):
        shutil.copytree(plugin_src, os.path.join(OUT, "plugin-macOS", "minColorCST.plugin"))
    pay = os.path.join(OUT, "minColor-data")
    os.makedirs(os.path.join(pay, "settings"))
    cfgs = os.path.join(pay, "configs")
    os.makedirs(cfgs)
    for name in os.listdir(DIST):
        srcp = os.path.join(DIST, name)
        if os.path.isdir(srcp):
            shutil.copytree(srcp, os.path.join(cfgs, name))
        else:
            shutil.copy(srcp, cfgs)
    shutil.copy(os.path.join(ROOT, "config", "extension-defaults.json"), os.path.join(pay, "settings"))
    shutil.copy(os.path.join(ROOT, "config", "render-presets.json"), os.path.join(pay, "settings"))
    ver = re.search(r'var VERSION = "([^"]+)"', open(os.path.join(SRC, "minColor.jsxinc"), encoding="utf-8").read()).group(1)
    zpath = os.path.join(OUT, "minColor-v%s.zip" % ver)
    with zipfile.ZipFile(zpath, "w", zipfile.ZIP_DEFLATED) as z:
        for dp, _, fs in os.walk(OUT):
            for f in fs:
                p = os.path.join(dp, f)
                if p == zpath: continue
                z.write(p, os.path.relpath(p, OUT))
    total = sum(os.path.getsize(os.path.join(dp, f)) for dp, _, fs in os.walk(OUT) for f in fs)
    print("dist-panel ready: v%s (%.1f MB, %s)" % (ver, total / 1e6, os.path.basename(zpath)))

if __name__ == "__main__":
    main()
