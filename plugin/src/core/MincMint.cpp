#include "MincCore.h"
#include <atomic>
#include <random>
#include <ctime>

/* Instance ids: unique across sessions (random per-session seed, then monotonic). A process-
   global counter starting at 1 minted duplicates across sessions; the registry is keyed by id,
   so two effects sharing one executed each other's transform (found 2026-08-29, SDR project).
   Per-binary counters (effect vs AEGP) are the same risk class as cross-session mints and are
   self-healed by the walk's idsThisWalk re-mint.                                              */
uint32_t MincMintInstanceId(void) {
    static std::atomic<uint32_t> ctr{0};
    if (ctr.load() == 0) {                                           /* lazy per-session seed */
        uint32_t seed = 0;
        try { std::random_device rd; seed = rd(); } catch (...) {}
        seed ^= (uint32_t)time(nullptr) * 2654435761u;
        if (seed == 0) seed = 0x9E3779B9u;
        uint32_t z = 0; ctr.compare_exchange_strong(z, seed);
    }
    uint32_t id = ctr.fetch_add(1);
    if (id == 0) id = ctr.fetch_add(1);
    return id;
}
