#include "MincMenusWrite.h"
#include "MincSuggest.h"
#include "MincPresets.h"
#include "MincSettings.h"
#include "MincFs.h"
#include "MincJson.h"
#include <cstdio>

static std::string JEsc(const std::string &s) {
    std::string o = "\"";
    for (char c : s) { if (c == '"' || c == '\\') o += '\\'; o += c; }
    return o + "\"";
}
static std::string JList(const std::vector<std::string> &v) {
    std::string o = "[";
    for (size_t i = 0; i < v.size(); ++i) { if (i) o += ", "; o += JEsc(v[i]); }
    return o + "]";
}

static std::string MenusPath(void) { return MincSettingsDir() + "/plugin-menus.json"; }

static bool WriteMenusFor(const std::string &preset, const std::string &pin) {
    MincMenuLists m = MincMenuListsFor(preset, pin);
    if (!m.valid) return false;
    std::string json = "{\n \"preset\": " + JEsc(m.preset) +
        ",\n \"family\": " + JEsc(m.family) +
        ",\n \"generatedBy\": " + JEsc(MINC_BUILD_STAMP) +
        ",\n \"defaultView\": " + JEsc(m.defView) +
        ",\n \"defaultRender\": " + JEsc(m.defRender) +
        ",\n \"inputSpaces\": " + JList(m.input) +
        ",\n \"viewSpaces\": " + JList(m.view) +
        ",\n \"renderSpaces\": " + JList(m.render) +
        ",\n \"looks\": " + JList(m.looks) + "\n}\n";
    std::string dst = MenusPath();
    std::string tmp = dst + ".tmp";
    if (!MincWriteTextFile(tmp, json)) { MincLog("menus: tmp write failed"); return false; }
    if (!mfs::replaceFile(tmp, dst)) { MincLog("menus: rename failed"); return false; }
    MincLog("menus: wrote plugin-menus.json preset=%s input=%d view=%d render=%d looks=%d",
            m.preset.c_str(), (int)m.input.size(), (int)m.view.size(), (int)m.render.size(), (int)m.looks.size());
    return true;
}

bool MincWriteMenus(SPBasicSuite *bp, AEGP_PluginID id) {
    (void)bp; (void)id;
    MincAuthoritySnapshot snap = {};
    MincAuthorityGet(&snap);
    /* lean-v3 Path 2: AE's pin is the lean INTERFACE config (1 space) — build the menus from the
       FULL config (the effect's config) so the panel dropdowns + christening defaults are complete. */
    std::string fullBase;
    std::string pin = MincEffectConfigPath(snap.configPath, "", &fullBase);
    std::string preset = MincPresetFromConfigBase(fullBase);
    if (preset.empty()) return false;                    /* not a minColor project: leave the file alone */
    return WriteMenusFor(preset, pin);
}

bool MincWriteMenusForPreset(const std::string &preset) {
    /* preset-keyed (2026-09-06): the Doctor knows the preset even when AE's pin does not resolve
       (fresh machine, other-OS path), so the menus can follow the DOCTOR, not only a ceremony.
       The "pin" handed to the list builder is the FULL config (store path or embedded basename),
       exactly what MincWriteMenus derives from a healthy pin. */
    MincPresetInfo pr = MincPresetMeta(preset);
    if (!pr.valid) return false;
    std::string pin = MincEffectConfigPath(pr.config, "", nullptr);
    return WriteMenusFor(preset, pin);
}

bool MincMenusFileIsFor(const std::string &preset) {
    MincJsonPtr j = MincJsonParseFile(MenusPath());
    return j && j->str("preset") == preset;
}
