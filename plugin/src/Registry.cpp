/* Instance registry — EFFECT BINARY ONLY, never in core: AE clones render-side sequence data
   from stale snapshots, so mutable state flows through this map keyed by the instanceId baked
   into the data. Writers reach it through CallGeneric (HandleGeneric), which always dispatches
   into THIS binary — the AEGP bundle acts on it without ever linking it.                     */
#include "MinColorCST.h"
#include <mutex>
#include <map>

static std::mutex g_regMx;
static std::map<uint32_t, MincSeqData> g_registry;

void MincRegistrySet(uint32_t id, const MincSeqData *sd) {
    if (!id) return;
    std::lock_guard<std::mutex> lk(g_regMx);
    g_registry[id] = *sd;
}
bool MincRegistryGet(uint32_t id, MincSeqData *out) {
    if (!id) return false;
    std::lock_guard<std::mutex> lk(g_regMx);
    auto it = g_registry.find(id);
    if (it == g_registry.end()) return false;
    *out = it->second;
    return true;
}
