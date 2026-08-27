/* Config + processor caches and row application. All OCIO 2.5 usage is confined here. */
#include "MincTypes.h"
#include <OpenColorIO/OpenColorIO.h>
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
        const char *src = (arb->direction == MINC_DIR_TO_WORKING) ? arb->space : auth->workingSpace;
        const char *dst = (arb->direction == MINC_DIR_TO_WORKING) ? auth->workingSpace : arb->space;
        if (!cfg->getColorSpace(src) || !cfg->getColorSpace(dst)) return MINC_STATUS_PASS_UNKNOWN_SPACE;
        return MINC_STATUS_OK;
    } catch (...) { return MINC_STATUS_PASS_CONFIG_ERROR; }
}

/* Token API: resolve ONCE per frame (the per-row path paid a stat + two map locks + string
   builds per row — ~2160 stats/frame at UHD). The token owns a heap shared_ptr so the
   processor survives cache clears for the duration of the frame. */
int MincOcioBegin(const MincAuthoritySnapshot *auth, const MinColorArb *arb, void **token) {
    *token = nullptr;
    if (!auth->ocioOn)          return MINC_STATUS_PASS_OCIO_OFF;
    if (!arb->space[0])         return MINC_STATUS_PASS_EMPTY;
    if (!auth->configPath[0] || !auth->workingSpace[0]) return MINC_STATUS_PASS_CONFIG_ERROR;
    try {
        OCIO::ConstConfigRcPtr cfg = GetConfig(auth->configPath);
        if (!cfg) return MINC_STATUS_PASS_CONFIG_ERROR;
        const char *src = (arb->direction == MINC_DIR_TO_WORKING) ? arb->space : auth->workingSpace;
        const char *dst = (arb->direction == MINC_DIR_TO_WORKING) ? auth->workingSpace : arb->space;
        if (!cfg->getColorSpace(src) || !cfg->getColorSpace(dst)) return MINC_STATUS_PASS_UNKNOWN_SPACE;
        std::string pkey = std::string(cfg->getCacheID()) + "|" + src + "|" + dst;
        OCIO::ConstCPUProcessorRcPtr proc;
        {
            std::shared_lock lk(g_procMx);
            auto it = g_procs.find(pkey);
            if (it != g_procs.end()) proc = it->second;
        }
        if (!proc) {
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
