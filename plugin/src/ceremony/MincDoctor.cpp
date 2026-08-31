#include "MincDoctor.h"
#include "MincPresets.h"
#include "MincJson.h"
#include "../core/MincRifx.h"
#include <sys/stat.h>
#include <map>

static const char *CENTRAL_ROOT = "/Library/Application Support/Adobe/Common/Plug-ins/7.0/MediaCore/minColor";

static bool FileExists(const std::string &p) { struct stat st; return !p.empty() && stat(p.c_str(), &st) == 0; }
static std::string BaseName(const std::string &p) {
    std::string s = p;
    for (size_t i = 0; i < s.size(); ++i) if (s[i] == '\\') s[i] = '/';
    size_t k = s.find_last_of('/');
    return k == std::string::npos ? s : s.substr(k + 1);
}
static std::string TrimCfg(const std::string &b) {       /* replace(/^config-|\.ocio$/g,"") */
    std::string s = b;
    if (s.compare(0, 7, "config-") == 0) s = s.substr(7);
    if (s.size() > 5 && s.compare(s.size() - 5, 5, ".ocio") == 0) s = s.substr(0, s.size() - 5);
    return s;
}
static std::string AfterSlash(const std::string &s) {    /* replace(/^[^\/]*\//,"") */
    size_t k = s.find('/');
    return k == std::string::npos ? s : s.substr(k + 1);
}
static std::string JsonStr(const std::string &s) {
    std::string o = "\"";
    for (size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        if (c == '"' || c == '\\') { o += '\\'; o += c; }
        else if (c == '\n') o += "\\n";
        else o += c;
    }
    return o + "\"";
}

std::string MincDoctorResult::toJson(void) const {
    std::string j = "{ \"status\": " + JsonStr(status) + ", \"text\": " + JsonStr(text) +
                    ", \"canRepair\": " + (canRepair ? "true" : "false") +
                    ", \"preset\": " + (preset.empty() ? std::string("null") : JsonStr(preset)) +
                    ", \"family\": " + JsonStr(family);
    if (!pin.empty()) j += ", \"pin\": " + JsonStr(pin);
    if (behind) j += ", \"pinBehind\": { \"pinned\": " + JsonStr(behindPinned) + ", \"current\": " + JsonStr(behindCurrent) + " }";
    return j + " }\n";
}

struct SidecarInfo { bool valid = false; std::string preset, configPath, config, pinned; };

/* port of sidecarInfo() :199-228 — provenance fallback reads the SAVED file's XMP trailer */
static SidecarInfo GetSidecarInfo(const std::string &pin, const std::string &projPath) {
    SidecarInfo out;
    std::string base = BaseName(pin);
    std::string preset = MincPresetFromConfigBase(base);
    if (!preset.empty()) {
        out.valid = true; out.preset = preset; out.config = base; out.pinned = pin;
        out.configPath = FileExists(pin) ? pin : "";
        return out;
    }
    if (!projPath.empty()) {
        std::string tail = MincReadFileTail(projPath.c_str());
        std::string prov = MincXmpReadElement(tail, "provenance");
        if (!prov.empty()) {
            MincJsonPtr pj = MincJsonParse(prov);
            if (pj && pj->has("config") && pj->has("preset")) {
                out.valid = true;
                out.preset = pj->str("preset");
                out.config = pj->str("config");
                out.configPath = MincFindConfigByName(out.config, projPath);
                return out;
            }
        }
        /* legacy fallback: _minColor/minColor.json keyed by display name */
        size_t s = projPath.find_last_of('/');
        if (s != std::string::npos) {
            std::string sd = projPath.substr(0, s) + "/_minColor";
            MincJsonPtr j = MincJsonParseFile(sd + "/minColor.json");
            if (j) {
                MincJsonPtr projects = j->get("projects");
                MincJsonPtr ent = projects ? projects->get(BaseName(projPath)) : nullptr;
                if (ent && ent->has("config")) {
                    out.valid = true;
                    out.preset = ent->str("preset");
                    out.config = ent->str("config");
                    std::string cf = sd + "/" + out.config;
                    out.configPath = FileExists(cf) ? cf : "";
                    return out;
                }
            }
        }
    }
    return out;
}

MincDoctorResult MincDoctorDiagnose(SPBasicSuite *bp, AEGP_PluginID id) {
    MincDoctorResult r;
    AEGP_SuiteHandler suites(bp);
    Acq<AEGP_ProjSuite6> pjs(bp, kAEGPProjSuite, kAEGPProjSuiteVersion6);
    Acq<AEGP_ItemSuite9> its(bp, kAEGPItemSuite, kAEGPItemSuiteVersion9);
    if (!pjs || !its) { r.status = "unmanaged"; r.text = "suite acquire failed"; return r; }

    AEGP_ProjectH projH = nullptr;
    pjs->AEGP_GetProjectByIndex(0, &projH);
    std::string projPath;
    if (projH) {
        AEGP_MemHandle ph = nullptr;
        if (pjs->AEGP_GetProjectPath(projH, &ph) == A_Err_NONE) {
            char buf[2048] = "";
            MincUtf16HandleToUtf8(suites, ph, buf, sizeof(buf));
            projPath = buf;
        }
    }

    /* live colour state (same getters the snapshot uses) */
    bool cmsOk = false;
    std::string pin, workingBare;
    {
        const void *csV = nullptr;
        if (bp->AcquireSuite(kAEGPColorSettingsSuite, kAEGPColorSettingsSuiteVersion5, &csV) == kSPNoError && csV) {
            const AEGP_ColorSettingsSuite5 *cs = (const AEGP_ColorSettingsSuite5 *)csV;
            A_Boolean on = FALSE;
            cs->AEGP_IsOCIOColorManagementUsed(id, &on);
            cmsOk = (on != FALSE);
            AEGP_MemHandle pH = nullptr, wH = nullptr;
            char buf[2048] = "";
            if (cs->AEGP_GetOCIOConfigurationFilePath(id, &pH) == A_Err_NONE) {
                MincUtf16HandleToUtf8(suites, pH, buf, sizeof(buf)); pin = buf;
            }
            if (cs->AEGPD_GetOCIOWorkingColorSpace(id, &wH) == A_Err_NONE) {
                buf[0] = 0; MincUtf16HandleToUtf8(suites, wH, buf, sizeof(buf)); workingBare = buf;
            }
            bp->ReleaseSuite(kAEGPColorSettingsSuite, kAEGPColorSettingsSuiteVersion5);
        }
    }

    SidecarInfo info = GetSidecarInfo(pin, projPath);
    if (!info.valid) {
        r.status = "unmanaged";
        r.text = projPath.empty() ? "unsaved project" : "not a minColor project (no pin, provenance, or sidecar)";
        r.preset = ""; r.family = "Linear";
        return r;
    }
    MincPresetInfo pr = MincPresetMeta(info.preset);
    r.preset = info.preset;
    r.family = MincFamilyFor(info.preset);
    r.pin = BaseName(pin);

    bool cfgOk = FileExists(pin) && !info.configPath.empty() && pin == info.configPath;
    bool wsOk = pr.valid ? (workingBare == pr.working) : (!workingBare.empty() && workingBare != "None");

    if (cmsOk && cfgOk && wsOk) {
        /* footage-level audit: reads the last-SAVED file (deliberate — same as panel :246-250) */
        if (!projPath.empty() && FileExists(projPath)) {
            MincAssignments a;
            if (MincRifxReadAssignments(projPath.c_str(), &a)) {
                std::map<int32_t, const MincAssignItem *> byId;
                for (auto &it2 : a.items) if (it2.id) byId[it2.id] = &it2;
                std::string wn = a.working.empty() ? "?" : AfterSlash(a.working);
                int bad = 0;
                /* live footage ids (byId only contains ocsp-assigned items, so the type
                   filter suffices — solids/placeholders never appear in it) */
                AEGP_ItemH itH = nullptr;
                its->AEGP_GetFirstProjItem(projH, &itH);
                while (itH) {
                    AEGP_ItemType ty = AEGP_ItemType_NONE;
                    its->AEGP_GetItemType(itH, &ty);
                    if (ty == AEGP_ItemType_FOOTAGE) {
                        A_long iid = 0;
                        its->AEGP_GetItemID(itH, &iid);
                        auto f = byId.find((int32_t)iid);
                        if (f != byId.end()) {
                            std::string cn = AfterSlash(f->second->colorspace);
                            if (cn != wn && cn != "default") ++bad;
                        }
                    }
                    AEGP_ItemH nx = nullptr;
                    its->AEGP_GetNextProjItem(projH, itH, &nx);
                    itH = nx;
                }
                if (bad > 0) {
                    r.status = "red";
                    char t[256];
                    snprintf(t, sizeof(t), "%d footage item(s) carry footage-level assignments \xe2\x80\x94 run Set Up / Migrate to strip", bad);
                    r.text = t;
                    return r;
                }
            }
        }
        std::string locus = (pin.compare(0, strlen(CENTRAL_ROOT), CENTRAL_ROOT) == 0) ? "central" : "sidecar";
        std::string bp2, bc;
        if (MincPinBehind(info.preset, BaseName(pin), &bp2, &bc)) {
            r.status = "yellow";
            r.behind = true; r.behindPinned = bp2; r.behindCurrent = bc;
            r.text = info.preset + " \xc2\xb7 pinned " + TrimCfg(bp2) + " \xe2\x86\x92 update available (" + TrimCfg(bc) + "): Set Up / Migrate, same preset";
            return r;
        }
        r.status = "green";
        std::string wsLabel = pr.valid ? pr.workingSpaceLabel : workingBare;
        r.text = info.preset + (pr.valid && pr.retired ? " (retired)" : "") + " \xc2\xb7 " + wsLabel + " \xc2\xb7 " + locus;
        return r;
    }
    if (!wsOk) {
        r.status = "red";
        r.text = "working space is " + (workingBare.empty() ? std::string("None") : workingBare) +
                 " (want " + (pr.valid ? pr.workingSpaceLabel : std::string("?")) + ") \xe2\x80\x94 run Set Up / Migrate";
        return r;
    }
    r.status = "yellow";
    r.text = std::string(!cmsOk ? "engine fell back to Adobe mode; " : "") + (!cfgOk ? "config path needs re-pointing" : "");
    r.canRepair = !info.configPath.empty() || !MincFindConfigByName(info.config, projPath).empty();
    return r;
}
