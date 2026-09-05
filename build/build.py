#!/usr/bin/env python3
"""Build the distributable (minColor 2.0 lean: plugin-first, one thin shell, embedded configs).

  dist-panel/minColor.jsx        the thin shell — ONE panel for both platforms (zero includes;
                                 keeps the 0.9.x install name so the Window menu entry and
                                 per-user install path stay stable). Gates on the AEGP handshake.
  dist-panel/plugin-macOS/       minColorCST.plugin (MediaCore) + minColorAEGP.plugin (app
                                 Plug-ins) + configs — the two-bundle engine, one version.
  dist-panel/plugin-windows/     minColor/minColorCST.aex (+ configs) + minColorAEGP.aex — the
                                 same two bundles, from plugin/prebuilt/windows (committed by the
                                 Windows session). The AEGP is present once the Windows port lands.

The effect embeds its OCIO configs, so the configs folder beside it is for the AEGP's disk reads,
not the effect. The 0.9.x windows-panel + minColor-data payload are retired: Windows runs the same
2.0 shell + AEGP as mac. Run packaging/macos/build-pkg.sh afterwards for the mac .pkg.
"""
import os, re, shutil, zipfile
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
SRC = os.path.join(ROOT, "src")
DIST = os.path.join(ROOT, "config", "dist")
OUT = os.path.join(ROOT, "dist-panel")

def plugin_version():
    m = re.search(r'set\(MINC_VERSION "([^"]+)"\)',
                  open(os.path.join(ROOT, "plugin", "CMakeLists.txt"), encoding="utf-8").read())
    return m.group(1)

def inline(panel_text, src_dir):
    """resolve #include lines; the 2.0 shell has none (no-op by design)"""
    def repl(m):
        inc = open(os.path.join(src_dir, m.group(1)), encoding="utf-8").read()
        return "// ==== inlined: %s ====\n%s\n" % (m.group(1), inc)
    return re.sub(r'#include\s+"([^"]+)"\s*\n', repl, panel_text)

def main():
    if os.path.exists(OUT):
        shutil.rmtree(OUT)
    os.makedirs(OUT)
    ver = plugin_version()                              # the plugin IS the product version (2.0)
    # ---- the one shell (both platforms), stable install name ----
    shell = open(os.path.join(SRC, "minColor Shell.jsx"), encoding="utf-8").read()
    open(os.path.join(OUT, "minColor.jsx"), "w", encoding="utf-8").write(inline(shell, SRC))

    # config-master-*.ocio are OCIO-2.5 QCView/Blender/tooling masters, NOT AE presets — they must
    # NOT reach AE's MediaCore config store (AE's OCIO <=2.4 can abort loading a 2.5 config;
    # RESULTS §40). The effect embeds its configs anyway; the store is for the AEGP's disk reads.
    AE_STORE_SKIP = shutil.ignore_patterns("config-master-*.ocio")

    # ---- macOS engine: effect (MediaCore) + AEGP (app Plug-ins) ----
    plugin_src = os.path.join(ROOT, "plugin", "build", "minColorCST.plugin")
    aegp_src = os.path.join(ROOT, "plugin", "build", "minColorAEGP.plugin")
    if os.path.isdir(plugin_src):
        mac_root = os.path.join(OUT, "plugin-macOS", "minColor")
        shutil.copytree(plugin_src, os.path.join(mac_root, "minColorCST.plugin"))
        shutil.copytree(DIST, os.path.join(mac_root, "configs"), ignore=AE_STORE_SKIP)
        if os.path.isdir(aegp_src):
            shutil.copytree(aegp_src, os.path.join(OUT, "plugin-macOS", "minColorAEGP.plugin"))

    # ---- Windows engine: effect .aex (MediaCore) + AEGP .aex (app Plug-ins) from prebuilt ----
    win_src = os.path.join(ROOT, "plugin", "prebuilt", "windows")   # Windows session commits Release .aex(es) + version.txt
    win_ver = "none"
    if os.path.isdir(win_src):
        vt = os.path.join(win_src, "version.txt")
        win_ver = open(vt, encoding="utf-8").read().strip() if os.path.exists(vt) else "none"
        win_media = os.path.join(OUT, "plugin-windows", "minColor")   # -> MediaCore\minColor
        os.makedirs(win_media)
        for name in ("minColorCST.aex", "version.txt"):
            p = os.path.join(win_src, name)
            if os.path.exists(p): shutil.copy(p, win_media)
        shutil.copytree(DIST, os.path.join(win_media, "configs"), ignore=AE_STORE_SKIP)
        aegp_aex = os.path.join(win_src, "minColorAEGP.aex")          # -> app Plug-ins (once the port lands)
        if os.path.exists(aegp_aex):
            shutil.copy(aegp_aex, os.path.join(OUT, "plugin-windows"))

    open(os.path.join(OUT, "README.txt"), "w", encoding="utf-8").write(
        "minColor " + ver + " — install\n"
        "engine: " + ver + " (mac) / " + win_ver + " (windows prebuilt)\n"
        "requires: After Effects 2025 or later\n\n"
        "The same three pieces on both platforms — effect, ceremonies (AEGP), and the panel.\n"
        "The old minColor-data payload is gone; the 2.0 shell doesn't use it (delete any you find).\n\n"
        "macOS (prefer the .pkg). By hand:\n"
        "1) EFFECT      plugin-macOS/minColor -> /Library/Application Support/Adobe/Common/Plug-ins/7.0/MediaCore/\n"
        "2) CEREMONIES  plugin-macOS/minColorAEGP.plugin -> /Applications/Adobe After Effects <ver>/Plug-ins/ (admin)\n"
        "3) PANEL       minColor.jsx -> ~/Library/Preferences/Adobe/After Effects/<ver>/Scripts/ScriptUI Panels/\n\n"
        "Windows (by hand):\n"
        "1) EFFECT      plugin-windows/minColor -> C:\\Program Files\\Adobe\\Common\\Plug-ins\\7.0\\MediaCore\\\n"
        "2) CEREMONIES  plugin-windows/minColorAEGP.aex -> C:\\Program Files\\Adobe\\Adobe After Effects <ver>\\Support Files\\Plug-ins\\\n"
        "3) PANEL       minColor.jsx -> %APPDATA%\\Adobe\\After Effects\\<ver>\\Scripts\\ScriptUI Panels\\\n\n"
        "Restart After Effects. The panel appears under Window > minColor.jsx.\n\n"
        "Your choices and settings live outside the install and survive updates:\n"
        "   macOS:   /Users/Shared/minColor/settings/\n"
        "   Windows: C:\\ProgramData\\minColor\\settings\\\n")

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
