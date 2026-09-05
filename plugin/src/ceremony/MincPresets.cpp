#include "MincPresets.h"
#include "MincCore.h"
#include "MincJson.h"
#include <sys/stat.h>

static bool FileExists(const std::string &p) { struct stat st; return stat(p.c_str(), &st) == 0; }

std::string MincCentralConfigsDir(void) {
#ifdef AE_OS_WIN
    return "C:/Program Files/Adobe/Common/Plug-ins/7.0/MediaCore/minColor/configs";
#else
    return "/Library/Application Support/Adobe/Common/Plug-ins/7.0/MediaCore/minColor/configs";
#endif
}

static std::string SharedConfigsDir(void) {
#ifdef AE_OS_WIN
    return "C:/ProgramData/minColor/configs";
#else
    return "/Users/Shared/minColor/configs";
#endif
}

static std::string     g_usedPath;
static MincJsonPtr     g_presets;
static bool            g_loaded = false;

static MincJsonPtr Load(void) {
    if (g_loaded) return g_presets;
    g_loaded = true;
    const std::string cands[] = { MincCentralConfigsDir() + "/presets.json",
                                  SharedConfigsDir() + "/presets.json" };
    for (int i = 0; i < 2; ++i) {
        MincJsonPtr j = MincJsonParseFile(cands[i]);
        if (j) { g_presets = j; g_usedPath = cands[i]; break; }
    }
    MincLog("presets: %s", g_usedPath.empty() ? "NOT FOUND" : g_usedPath.c_str());
    return g_presets;
}

std::string MincPresetsFileUsed(void) { Load(); return g_usedPath; }

MincPresetInfo MincPresetMeta(const std::string &key) {
    MincPresetInfo out;
    MincJsonPtr j = Load();
    if (!j || key.empty()) return out;
    MincJsonPtr rec = nullptr;
    MincJsonPtr live = j->get("presets");
    if (live) rec = live->get(key);
    if (!rec) {
        MincJsonPtr ret = j->get("retired");
        if (ret) { rec = ret->get(key); if (rec) out.retired = true; }
    }
    if (!rec) return out;
    out.valid = true;
    out.key = key;
    out.config = rec->str("config");
    out.working = rec->str("working");
    out.family = rec->str("family", "Linear");
    out.label = rec->str("label");
    out.workingSpaceLabel = rec->str("workingSpaceLabel");
    out.pwcsJSON = rec->str("pwcsJSON");
    return out;
}

std::string MincFamilyFor(const std::string &key) {
    MincPresetInfo m = MincPresetMeta(key);
    return (m.valid && !m.family.empty()) ? m.family : "Linear";
}

std::string MincPresetFromConfigBase(const std::string &baseIn) {
    /* lean-v3 Path 2: a lean INTERFACE pin (config-<preset>-interface.ocio) identifies the same
       managed project as its full config — strip the suffix first so detection/preset-mapping
       recognize it everywhere (Doctor, Archive/Package/Adopt, behind-check, menus). */
    char st[MINC_CONFIGBASE_LEN]; MincPassportConfigBase(baseIn.c_str(), st, sizeof(st));
    std::string base = st[0] ? std::string(st) : baseIn;
    /* Accepts BOTH the stable name config-<preset>.ocio (2026-09-03) and the legacy hashed
       config-<preset>-<hex>.ocio (old dev/shipped pins still resolve their preset). Preset
       keys are alphanumeric (no dash), so a trailing "-<hex>" is unambiguously a legacy hash. */
    const std::string pre = "config-", suf = ".ocio";
    if (base.size() <= pre.size() + suf.size()) return "";
    if (base.compare(0, pre.size(), pre) != 0) return "";
    if (base.compare(base.size() - suf.size(), suf.size(), suf) != 0) return "";
    std::string mid = base.substr(pre.size(), base.size() - pre.size() - suf.size());
    std::string preset = mid;
    size_t dash = mid.rfind('-');
    if (dash != std::string::npos && dash > 0 && dash + 1 < mid.size()) {
        std::string tail = mid.substr(dash + 1);
        bool hex = true;
        for (char c : tail) if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) { hex = false; break; }
        if (hex) preset = mid.substr(0, dash);           /* legacy hash suffix -> strip it */
    }
    for (size_t i = 0; i < preset.size(); ++i)
        if (!isalnum((unsigned char)preset[i])) return "";
    return preset;
}

bool MincPinBehind(const std::string &presetKey, const std::string &pinnedBaseIn,
                   std::string *pinnedOut, std::string *currentOut) {
    MincPresetInfo m = MincPresetMeta(presetKey);
    if (!m.valid || m.retired || pinnedBaseIn.empty()) return false;   /* mirror pinBehind(): live presets only */
    char st[MINC_CONFIGBASE_LEN]; MincPassportConfigBase(pinnedBaseIn.c_str(), st, sizeof(st));
    std::string pinnedBase = st[0] ? std::string(st) : pinnedBaseIn;   /* lean-v3: interface pin is current, not behind */
    if (pinnedBase == m.config) return false;
    if (pinnedOut) *pinnedOut = pinnedBase;
    if (currentOut) *currentOut = m.config;
    return true;
}

std::string MincFindConfigByName(const std::string &base, const std::string &projPath) {
    std::string cands[3];
    int n = 0;
    cands[n++] = MincCentralConfigsDir() + "/" + base;
    if (!projPath.empty()) {
        size_t s = projPath.find_last_of('/');
        if (s != std::string::npos) cands[n++] = projPath.substr(0, s) + "/_minColor/" + base;
    }
    cands[n++] = SharedConfigsDir() + "/" + base;
    for (int i = 0; i < n; ++i) if (FileExists(cands[i])) return cands[i];
    return "";
}

std::string MincEffectConfigPath(const std::string &pinnedPath, const std::string &projPath,
                                 std::string *fullBaseOut) {
    char fb[MINC_CONFIGBASE_LEN] = "";
    MincPassportConfigBase(pinnedPath.c_str(), fb, sizeof(fb));   /* basename, -interface stripped */
    std::string fullBase = fb[0] ? std::string(fb) : pinnedPath;
    if (fullBaseOut) *fullBaseOut = fullBase;
    std::string p = MincFindConfigByName(fullBase, projPath);
    return p.empty() ? pinnedPath : p;                           /* fall back to the pin if unresolved */
}
