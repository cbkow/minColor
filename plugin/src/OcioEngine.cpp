/* Config + processor caches and row application. All OCIO 2.5 usage is confined here. */
#include "MincTypes.h"
#include <OpenColorIO/OpenColorIO.h>
#include <shared_mutex>
#include <map>
#include <string>
#include <sys/stat.h>
namespace OCIO = OCIO_NAMESPACE;

struct ConfigEntry { std::string key; OCIO::ConstConfigRcPtr config; };
static std::shared_mutex g_cfgMx;
static std::map<std::string, ConfigEntry> g_configs;       /* path -> entry (key = path|mtime|size) */
static std::shared_mutex g_procMx;
static std::map<std::string, OCIO::ConstCPUProcessorRcPtr> g_procs;

static std::string StatKey(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return std::string();
    return std::string(path) + "|" + std::to_string(st.st_mtime) + "|" + std::to_string(st.st_size);
}

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

int MincOcioApplyRows(const MincAuthoritySnapshot *auth, const MinColorArb *arb,
                      float *rgbaRows, int pixelCount) {
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
        OCIO::PackedImageDesc desc(rgbaRows, pixelCount, 1, 4);        /* RGBA float rows */
        proc->apply(desc);                                             /* thread-safe, stateless */
        return MINC_STATUS_OK;
    } catch (...) { return MINC_STATUS_PASS_CONFIG_ERROR; }
}
