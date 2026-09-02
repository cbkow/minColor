#include "MincArchive.h"
#include "MincPresets.h"
#include "MincSettings.h"
#include "MincJson.h"
#include "../core/MincRifx.h"
#include <cstdio>
#include <dirent.h>
#include <sys/stat.h>

static bool AExists(const std::string &p) { struct stat st; return !p.empty() && stat(p.c_str(), &st) == 0; }
static bool AIsDir(const std::string &p) { struct stat st; return stat(p.c_str(), &st) == 0 && (st.st_mode & S_IFDIR); }
static std::string AJStr(const std::string &s) {
    std::string o = "\"";
    for (char c : s) { if (c == '"' || c == '\\') o += '\\'; o += c; }
    return o + "\"";
}
static bool ACopy(const std::string &from, const std::string &to) {
    FILE *a = fopen(from.c_str(), "rb");
    if (!a) return false;
    FILE *b = fopen(to.c_str(), "wb");
    if (!b) { fclose(a); return false; }
    char buf[65536]; size_t n; bool ok = true;
    while ((n = fread(buf, 1, sizeof(buf), a)) > 0) if (fwrite(buf, 1, n, b) != n) { ok = false; break; }
    fclose(a); fclose(b);
    return ok;
}
static void ACopyTree(const std::string &from, const std::string &to) {   /* copyTree port: skip existing files */
    if (!AIsDir(from)) return;
    mkdir(to.c_str(), 0777);
    DIR *d = opendir(from.c_str());
    if (!d) return;
    struct dirent *de;
    while ((de = readdir(d)) != nullptr) {
        std::string n = de->d_name;
        if (n == "." || n == "..") continue;
        std::string src = from + "/" + n, dst = to + "/" + n;
        if (AIsDir(src)) ACopyTree(src, dst);
        else if (!AExists(dst)) ACopy(src, dst);
    }
    closedir(d);
}

bool MincEnsureSidecar(const std::string &projPath, const std::string &preset,
                       std::string *cfgOut, std::string *errOut) {
    MincPresetInfo pr = MincPresetMeta(preset);
    if (!pr.valid) { if (errOut) *errOut = "unknown preset " + preset; return false; }
    size_t sl = projPath.find_last_of('/');
    std::string projDir = projPath.substr(0, sl);
    std::string projName = projPath.substr(sl + 1);
    std::string sd = projDir + "/_minColor";
    mkdir(sd.c_str(), 0777);
    std::string root = MincCentralConfigsDir();
    const char *trees[] = { "luts", "filmic", "icc" };
    for (int i = 0; i < 3; ++i) ACopyTree(root + "/" + trees[i], sd + "/" + trees[i]);
    std::string cfgTarget = sd + "/" + pr.config;
    if (!AExists(cfgTarget)) {
        if (!ACopy(root + "/" + pr.config, cfgTarget)) { if (errOut) *errOut = "config copy failed"; return false; }
    }
    {   /* minColor.json v2 merge: per-project entries keyed by display name */
        MincJsonPtr j = MincJsonParseFile(sd + "/minColor.json");
        std::string parts;
        bool first = true;
        auto addEntry = [&](const std::string &k, const std::string &pk, const std::string &cf, const std::string &pv) {
            std::string esc;
            for (char c : k) { if (c == '"' || c == '\\') esc += '\\'; esc += c; }
            if (!first) parts += ",\n  ";
            parts += "\"" + esc + "\": { \"preset\": \"" + pk + "\", \"config\": \"" + cf + "\", \"panel\": \"" + pv + "\" }";
            first = false;
        };
        if (j) {
            MincJsonPtr projects = j->get("projects");
            if (projects && projects->type == MincJsonValue::Object)
                for (auto &kv : projects->obj)
                    if (kv.second && kv.first != projName && !kv.second->str("config").empty())
                        addEntry(kv.first, kv.second->str("preset"), kv.second->str("config"), kv.second->str("panel", MINC_VERSION_STR));
        }
        addEntry(projName, preset, pr.config, MINC_VERSION_STR);
        MincWriteTextFile(sd + "/minColor.json", "{ \"projects\": {\n  " + parts + "\n} }");
    }
    if (cfgOut) *cfgOut = cfgTarget;
    return true;
}

std::string MincArchiveProject(SPBasicSuite *bp, AEGP_PluginID id) {
    AEGP_SuiteHandler suites(bp);
    Acq<AEGP_ProjSuite6> pjs(bp, kAEGPProjSuite, kAEGPProjSuiteVersion6);
    if (!pjs) return "{ \"error\": \"suite acquire failed\" }\n";
    AEGP_ProjectH projH = nullptr;
    pjs->AEGP_GetProjectByIndex(0, &projH);
    std::string projPath;
    if (projH) {
        AEGP_MemHandle ph = nullptr;
        char b[2048] = "";
        if (pjs->AEGP_GetProjectPath(projH, &ph) == A_Err_NONE)
            MincUtf16HandleToUtf8(suites, ph, b, sizeof(b));
        projPath = b;
    }
    if (projPath.empty()) return "{ \"error\": \"save the project first\" }\n";
    /* identity from the pin (sidecarInfo semantics — CONFIG_PATTERN on the live pin) */
    MincAuthorityRefreshBp(bp, id);
    MincAuthoritySnapshot snap = {};
    MincAuthorityGet(&snap);
    std::string pin = snap.configPath;
    std::string pinBase = pin.substr(pin.find_last_of('/') == std::string::npos ? 0 : pin.find_last_of('/') + 1);
    std::string preset = MincPresetFromConfigBase(pinBase);
    if (preset.empty()) return "{ \"error\": \"not a minColor project\" }\n";
    MincPresetInfo pr = MincPresetMeta(preset);
    if (!pr.valid) return "{ \"error\": \"unknown preset\" }\n";

    /* ensureSidecar port (:177-196) — trees + config beside the project, minColor.json merge */
    std::string cfgTarget, serr;
    if (!MincEnsureSidecar(projPath, preset, &cfgTarget, &serr))
        return "{ \"error\": " + AJStr(serr) + " }\n";
    size_t sl = projPath.find_last_of('/');
    std::string sd = projPath.substr(0, sl) + "/_minColor";
    /* provenance.json from the SAVED file's XMP (:504-507) */
    std::string prov = MincXmpReadElement(MincReadFileTail(projPath.c_str()), "provenance");
    if (prov.empty()) prov = "{}";
    std::string pf = sd + "/provenance.json";
    MincWriteTextFile(pf, prov);

    /* no "golden" field: the panel's golden render retired with the panel (M2 decision) */
    return "{ \"sidecar\": " + AJStr(cfgTarget) + ", \"provenance\": " + AJStr(pf) + " }\n";
}
