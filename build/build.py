#!/usr/bin/env python3
"""Build the distributable: single-file panel + payload staging.

  dist-panel/minColor.jsx      panel with both .jsxinc files inlined (no #include)
  dist-panel/payload/          configs (hashed, + luts/filmic/icc), presets.json,
                               settings/extension-defaults.json
Run install.command afterwards (or copy by hand).
"""
import os, re, shutil
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
        "minColor — install\n\n"
        "Copy BOTH items into your After Effects ScriptUI Panels folder:\n\n"
        "    minColor.jsx\n    minColor-data/\n\n"
        "macOS:   ~/Library/Preferences/Adobe/After Effects/<version>/Scripts/ScriptUI Panels/\n"
        "         (or the app's Scripts/ScriptUI Panels folder)\n"
        "Windows: %APPDATA%\\Adobe\\After Effects\\<version>\\Scripts\\ScriptUI Panels\\\n"
        "         (or Program Files\\Adobe\\...\\Support Files\\Scripts\\ScriptUI Panels\\, needs admin)\n\n"
        "Restart After Effects; the panel appears under Window > minColor.jsx.\n")
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
    total = sum(os.path.getsize(os.path.join(dp, f)) for dp, _, fs in os.walk(OUT) for f in fs)
    print("dist-panel ready (%.1f MB)" % (total / 1e6))

if __name__ == "__main__":
    main()
