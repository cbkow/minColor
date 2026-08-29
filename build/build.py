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
    win_ver_path = os.path.join(ROOT, "plugin", "prebuilt", "windows", "version.txt")
    win_ver = open(win_ver_path, encoding="utf-8").read().strip() if os.path.exists(win_ver_path) else "none"
    open(os.path.join(OUT, "minColor.jsx"), "w", encoding="utf-8").write(inline(panel))
    open(os.path.join(OUT, "README.txt"), "w", encoding="utf-8").write(
        "minColor — install\n"
        "panel: " + re.search(r'var VERSION = \"([^\"]+)\"', open(os.path.join(SRC, "minColor.jsxinc"), encoding="utf-8").read()).group(1) + "   engine: 1.3.1 (mac) / " + win_ver + " (windows prebuilt)\n"
        "requires: After Effects 2025 or later\n\n"
        "Two copy steps. Nothing to create — if a minColor folder already exists,\n"
        "let your OS merge/replace.\n\n"
        "1) PANEL — copy BOTH items into your ScriptUI Panels folder:\n\n"
        "       minColor.jsx\n"
        "       minColor-data/\n\n"
        "   macOS:   ~/Library/Preferences/Adobe/After Effects/<version>/Scripts/ScriptUI Panels/\n"
        "   Windows: %APPDATA%\\Adobe\\After Effects\\<version>\\Scripts\\ScriptUI Panels\\\n\n"
        "2) ENGINE — copy the minColor folder for your platform into Adobe's shared\n"
        "   MediaCore plug-ins folder:\n\n"
        "   macOS:   plugin-macOS/minColor   ->  /Library/Application Support/Adobe/Common/Plug-ins/7.0/MediaCore/\n"
        "   Windows: plugin-windows/minColor ->  C:\\Program Files\\Adobe\\Common\\Plug-ins\\7.0\\MediaCore\\\n\n"
        "Restart After Effects. The panel appears under Window > minColor.jsx.\n\n"
        "Your choices and settings live outside the install and survive updates:\n"
        "   macOS:   /Users/Shared/minColor/settings/\n"
        "   Windows: C:\\ProgramData\\minColor\\settings\\\n")
    # engine dirs mirror their DESTINATION: users copy "minColor" into MediaCore and the
    # OS merges — no folder creation, no separate configs step (each carries the store).
    plugin_src = os.path.join(ROOT, "plugin", "build", "minColorCST.plugin")
    if os.path.isdir(plugin_src):
        mac_root = os.path.join(OUT, "plugin-macOS", "minColor")
        shutil.copytree(plugin_src, os.path.join(mac_root, "minColorCST.plugin"))
        shutil.copytree(DIST, os.path.join(mac_root, "configs"))
    win_src = os.path.join(ROOT, "plugin", "prebuilt", "windows")   # Windows session commits Release .aex + version.txt here
    if os.path.isdir(win_src):
        win_root = os.path.join(OUT, "plugin-windows", "minColor")
        shutil.copytree(win_src, win_root)
        shutil.copytree(DIST, os.path.join(win_root, "configs"))
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
