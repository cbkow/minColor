#!/usr/bin/env python3
"""Build the distributable (minColor 2.0: plugin-first, thin shell).

  dist-panel/minColor.jsx        the thin shell (zero includes — the 2.0 panel IS one file;
                                 keeps the 0.9.x install name so the Window menu entry and
                                 per-user install path stay stable)
  dist-panel/plugin-macOS/       minColorCST.plugin (MediaCore) + minColorAEGP.plugin (app
                                 Plug-ins) + configs — the two-bundle engine, one version
  dist-panel/plugin-windows/     prebuilt .aex (five effects incl. legacy) + configs
  dist-panel/windows-panel/      the 0.9.2 panel, inlined from private/attic — Windows has
                                 no AEGP until M4, so the legacy panel keeps shipping there
  dist-panel/minColor-data/      shared payload: configs + settings seeds

Run packaging/macos/build-pkg.sh afterwards for the mac .pkg (or copy by hand).
"""
import os, re, shutil, zipfile
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
SRC = os.path.join(ROOT, "src")
ATTIC = os.path.join(ROOT, "private", "attic")
DIST = os.path.join(ROOT, "config", "dist")
OUT = os.path.join(ROOT, "dist-panel")

def plugin_version():
    m = re.search(r'set\(MINC_VERSION "([^"]+)"\)',
                  open(os.path.join(ROOT, "plugin", "CMakeLists.txt"), encoding="utf-8").read())
    return m.group(1)

def inline(panel_text, src_dir):
    """resolve #include lines; the shell has none (no-op by design), the attic panel has two"""
    def repl(m):
        inc = open(os.path.join(src_dir, m.group(1)), encoding="utf-8").read()
        return "// ==== inlined: %s ====\n%s\n" % (m.group(1), inc)
    return re.sub(r'#include\s+"([^"]+)"\s*\n', repl, panel_text)

def main():
    if os.path.exists(OUT):
        shutil.rmtree(OUT)
    os.makedirs(OUT)
    ver = plugin_version()                              # the plugin IS the product version (2.0)
    # ---- mac panel: the thin shell, stable install name ----
    shell = open(os.path.join(SRC, "minColor Shell.jsx"), encoding="utf-8").read()
    open(os.path.join(OUT, "minColor.jsx"), "w", encoding="utf-8").write(inline(shell, SRC))
    # ---- windows panel lane (0.9.2, from the attic — retires in M4) ----
    win_panel_src = os.path.join(ATTIC, "minColor Panel.jsx")
    if os.path.exists(win_panel_src):
        wp = os.path.join(OUT, "windows-panel")
        os.makedirs(wp)
        open(os.path.join(wp, "minColor.jsx"), "w", encoding="utf-8").write(
            inline(open(win_panel_src, encoding="utf-8").read(), ATTIC))
    win_ver_path = os.path.join(ROOT, "plugin", "prebuilt", "windows", "version.txt")
    win_ver = open(win_ver_path, encoding="utf-8").read().strip() if os.path.exists(win_ver_path) else "none"
    open(os.path.join(OUT, "README.txt"), "w", encoding="utf-8").write(
        "minColor " + ver + " — install\n"
        "engine: " + ver + " (mac) / " + win_ver + " (windows prebuilt)\n"
        "requires: After Effects 2025 or later\n\n"
        "macOS (2.0): prefer the .pkg. By hand, three copies:\n\n"
        "1) EFFECT — plugin-macOS/minColor -> /Library/Application Support/Adobe/Common/Plug-ins/7.0/MediaCore/\n"
        "2) CEREMONIES — plugin-macOS/minColorAEGP.plugin -> /Applications/Adobe After Effects <version>/Plug-ins/\n"
        "   (every AE version you use; this folder needs an administrator)\n"
        "3) PANEL — minColor.jsx and minColor-data/ ->\n"
        "   ~/Library/Preferences/Adobe/After Effects/<version>/Scripts/ScriptUI Panels/\n\n"
        "Windows (0.9.x panel until the 2.0 engine arrives there):\n\n"
        "1) PANEL — windows-panel/minColor.jsx and minColor-data/ ->\n"
        "   %APPDATA%\\Adobe\\After Effects\\<version>\\Scripts\\ScriptUI Panels\\\n"
        "2) ENGINE — plugin-windows/minColor -> C:\\Program Files\\Adobe\\Common\\Plug-ins\\7.0\\MediaCore\\\n\n"
        "Restart After Effects. The panel appears under Window > minColor.jsx.\n\n"
        "Your choices and settings live outside the install and survive updates:\n"
        "   macOS:   /Users/Shared/minColor/settings/\n"
        "   Windows: C:\\ProgramData\\minColor\\settings\\\n")
    # engine dirs mirror their DESTINATION: users copy "minColor" into MediaCore and the
    # OS merges — no folder creation, no separate configs step (each carries the store).
    plugin_src = os.path.join(ROOT, "plugin", "build", "minColorCST.plugin")
    aegp_src = os.path.join(ROOT, "plugin", "build", "minColorAEGP.plugin")
    if os.path.isdir(plugin_src):
        mac_root = os.path.join(OUT, "plugin-macOS", "minColor")
        shutil.copytree(plugin_src, os.path.join(mac_root, "minColorCST.plugin"))
        shutil.copytree(DIST, os.path.join(mac_root, "configs"))
        if os.path.isdir(aegp_src):                     # the AEGP sits BESIDE the MediaCore dir in
            shutil.copytree(aegp_src, os.path.join(OUT, "plugin-macOS", "minColorAEGP.plugin"))
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
