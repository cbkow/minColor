/* Config + processor caches and row application. All OCIO 2.5 usage is confined here. */
#include "MincTypes.h"
#include <OpenColorIO/OpenColorIO.h>
#include <cstdio>
#include <shared_mutex>
#include <map>
#include <string>
namespace OCIO = OCIO_NAMESPACE;

struct ConfigEntry { std::string key; OCIO::ConstConfigRcPtr config; };
static std::shared_mutex g_cfgMx;
static std::map<std::string, ConfigEntry> g_configs;       /* path -> entry (key = path|mtime|size) */
static std::shared_mutex g_procMx;
static std::map<std::string, OCIO::ConstCPUProcessorRcPtr> g_procs;

#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <sys/stat.h>
static std::string StatKey(const char *path) {               /* UTF-8 -> wide: ANSI stat() fails on non-ASCII paths */
    wchar_t w[2048];
    if (!MultiByteToWideChar(CP_UTF8, 0, path, -1, w, 2048)) return std::string();
    struct _stat64 st;
    if (_wstat64(w, &st) != 0) return std::string();
    return std::string(path) + "|" + std::to_string(st.st_mtime) + "|" + std::to_string(st.st_size);
}
#else
#include <sys/stat.h>
static std::string StatKey(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return std::string();
    return std::string(path) + "|" + std::to_string(st.st_mtime) + "|" + std::to_string(st.st_size);
}
#endif

static OCIO::ConstConfigRcPtr GetConfig(const char *path) {
    std::string key = StatKey(path);
    if (key.empty()) return nullptr;
    {
        std::shared_lock lk(g_cfgMx);
        auto it = g_configs.find(path);
        if (it != g_configs.end() && it->second.key == key) return it->second.config;
    }
    OCIO::ConstConfigRcPtr cfg = OCIO::Config::CreateFromFile(path);   /* throws on failure */
    std::unique_lock lk(g_cfgMx);
    if (g_configs.size() > 4) g_configs.clear();                        /* tiny LRU-ish bound */
    g_configs[path] = { key, cfg };
    return cfg;
}

int MincOcioProbeStatus(const MincAuthoritySnapshot *auth, const MinColorArb *arb) {
    if (!auth->ocioOn)  return MINC_STATUS_PASS_OCIO_OFF;
    if (!arb->space[0]) return MINC_STATUS_PASS_EMPTY;
    if (!auth->configPath[0] || !auth->workingSpace[0]) return MINC_STATUS_PASS_CONFIG_ERROR;
    try {
        OCIO::ConstConfigRcPtr cfg = GetConfig(auth->configPath);
        if (!cfg) return MINC_STATUS_PASS_CONFIG_ERROR;
        if (arb->direction == MINC_DIR_LOOK) {
            if (!cfg->getLook(arb->space)) return MINC_STATUS_PASS_UNKNOWN_SPACE;
            if (!cfg->getColorSpace(auth->workingSpace)) return MINC_STATUS_PASS_UNKNOWN_SPACE;
            return MINC_STATUS_OK;
        }
        const char *src = (arb->direction == MINC_DIR_TO_WORKING) ? arb->space : auth->workingSpace;
        const char *dst = (arb->direction == MINC_DIR_TO_WORKING) ? auth->workingSpace : arb->space;
        if (!cfg->getColorSpace(src) || !cfg->getColorSpace(dst)) return MINC_STATUS_PASS_UNKNOWN_SPACE;
        return MINC_STATUS_OK;
    } catch (...) { return MINC_STATUS_PASS_CONFIG_ERROR; }
}

/* Token API: resolve ONCE per frame (the per-row path paid a stat + two map locks + string
   builds per row — ~2160 stats/frame at UHD). The token owns a heap shared_ptr so the
   processor survives cache clears for the duration of the frame. */
/* The effect is self-reliant (Pole A): AE holds the config only to NEUTRALIZE its own colour
   management (working space None), and hands us neutral pixels. So "working" comes from AE's
   authority ONLY if it's actually set; otherwise the effect defines it from the config it
   renders with — the config's scene_linear role. Never depends on AE's working space. */
static void ResolveWorking(const MincAuthoritySnapshot *auth, const OCIO::ConstConfigRcPtr &cfg,
                           char out[MINC_SPACE_LEN]) {
    out[0] = 0;
    if (auth->workingSpace[0]) { snprintf(out, MINC_SPACE_LEN, "%s", auth->workingSpace); return; }
    try {
        OCIO::ConstColorSpaceRcPtr ws = cfg->getColorSpace(OCIO::ROLE_SCENE_LINEAR);
        if (ws) snprintf(out, MINC_SPACE_LEN, "%s", ws->getName());
    } catch (...) {}
}

int MincOcioBegin(const MincAuthoritySnapshot *auth, const MinColorArb *arb, void **token) {
    *token = nullptr;
    if (!auth->ocioOn)          return MINC_STATUS_PASS_OCIO_OFF;
    if (!arb->space[0])         return MINC_STATUS_PASS_EMPTY;
    if (!auth->configPath[0])   return MINC_STATUS_PASS_CONFIG_ERROR;
    try {
        OCIO::ConstConfigRcPtr cfg = GetConfig(auth->configPath);
        if (!cfg) return MINC_STATUS_PASS_CONFIG_ERROR;
        char working[MINC_SPACE_LEN]; ResolveWorking(auth, cfg, working);
        if (!working[0]) return MINC_STATUS_PASS_CONFIG_ERROR;
        bool isLook = (arb->direction == MINC_DIR_LOOK);
        const char *src = isLook ? working : (arb->direction == MINC_DIR_TO_WORKING) ? arb->space : working;
        const char *dst = isLook ? working : (arb->direction == MINC_DIR_TO_WORKING) ? working : arb->space;
        if (isLook) {
            if (!cfg->getLook(arb->space) || !cfg->getColorSpace(working)) return MINC_STATUS_PASS_UNKNOWN_SPACE;
        } else if (!cfg->getColorSpace(src) || !cfg->getColorSpace(dst)) return MINC_STATUS_PASS_UNKNOWN_SPACE;
        /* dirTag prevents key collisions: a LOOK (src==dst==working) must never share a cache
           entry with an identity colorspace transform in the same working space */
        std::string pkey = std::string(cfg->getCacheID()) + "|d" + std::to_string((int)arb->direction)
                         + "|" + src + "|" + dst + (isLook ? std::string("|") + arb->space : std::string());
        OCIO::ConstCPUProcessorRcPtr proc;
        {
            std::shared_lock lk(g_procMx);
            auto it = g_procs.find(pkey);
            if (it != g_procs.end()) proc = it->second;
        }
        if (!proc) {
            if (isLook) {
                OCIO::LookTransformRcPtr lt = OCIO::LookTransform::Create();
                lt->setSrc(src); lt->setDst(dst); lt->setLooks(arb->space);
                proc = cfg->getProcessor(lt)->getDefaultCPUProcessor();
            } else
                proc = cfg->getProcessor(src, dst)->getDefaultCPUProcessor();
            std::unique_lock lk(g_procMx);
            if (g_procs.size() > 64) g_procs.clear();
            g_procs[pkey] = proc;
        }
        *token = new OCIO::ConstCPUProcessorRcPtr(proc);
        return MINC_STATUS_OK;
    } catch (...) { return MINC_STATUS_PASS_CONFIG_ERROR; }
}
void MincOcioApplyToken(void *token, float *rgbaRows, int pixelCount) {
    if (!token) return;
    OCIO::PackedImageDesc desc(rgbaRows, pixelCount, 1, 4);            /* RGBA float rows */
    (*reinterpret_cast<OCIO::ConstCPUProcessorRcPtr*>(token))->apply(desc);   /* thread-safe, stateless */
}
/* badge-menu fallback: enumerate the EFFECTIVE config when plugin-menus.json is absent.
   Unfiltered (no per-preset curation without the panel's file) — assertSpaceInPin semantics
   still hold because everything offered exists in the config that will render it. */
int MincOcioListSpaces(const MincAuthoritySnapshot *auth, char out[][MINC_SPACE_LEN], int maxN) {
    if (!auth->ocioOn || !auth->configPath[0]) return 0;
    try {
        OCIO::ConstConfigRcPtr cfg = GetConfig(auth->configPath);
        if (!cfg) return 0;
        int n = 0, total = cfg->getNumColorSpaces();
        for (int i = 0; i < total && n < maxN; ++i) {
            const char *nm = cfg->getColorSpaceNameByIndex(i);
            if (nm && nm[0]) { snprintf(out[n], MINC_SPACE_LEN, "%s", nm); ++n; }
        }
        return n;
    } catch (...) { return 0; }
}
int MincOcioListLooks(const MincAuthoritySnapshot *auth, char out[][MINC_SPACE_LEN], int maxN) {
    if (!auth->ocioOn || !auth->configPath[0]) return 0;
    try {
        OCIO::ConstConfigRcPtr cfg = GetConfig(auth->configPath);
        if (!cfg) return 0;
        int n = 0, total = cfg->getNumLooks();
        for (int i = 0; i < total && n < maxN; ++i) {
            const char *nm = cfg->getLookNameByIndex(i);
            if (nm && nm[0]) { snprintf(out[n], MINC_SPACE_LEN, "%s", nm); ++n; }
        }
        return n;
    } catch (...) { return 0; }
}

void MincOcioEnd(void *token) {
    delete reinterpret_cast<OCIO::ConstCPUProcessorRcPtr*>(token);
}

int MincOcioApplyRows(const MincAuthoritySnapshot *auth, const MinColorArb *arb,
                      float *rgbaRows, int pixelCount) {               /* kept for tools/probe-engine */
    void *tok = nullptr;
    int st = MincOcioBegin(auth, arb, &tok);
    if (st == MINC_STATUS_OK) { try { MincOcioApplyToken(tok, rgbaRows, pixelCount); } catch (...) { st = MINC_STATUS_PASS_CONFIG_ERROR; } }
    MincOcioEnd(tok);
    return st;
}
