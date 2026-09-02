/* Cross-bundle walk signal — the effect binary cannot call into the AEGP binary (separate
   dylibs, per-binary statics), so "an instance appeared" travels as a marker FILE in the
   shared settings dir. Effect touches it on fresh SEQUENCE_SETUP (reason "christen") and
   after a badge edit (reason "sync", step 5); the AEGP idle stats it each ~1s tick and
   consumes DELETE-FIRST, so a drop landing mid-walk re-creates it for the next tick — no
   lost signals. Reasons never downgrade: "christen" ⊃ "sync". Stale markers are benign by
   construction (christening only renames default-named fresh variants from menus defaults —
   idempotent no-op on a settled project).                                                 */
#pragma once
#include <cstddef>

void MincTouchWalkMarker(const char *reason);            /* rate-limited ~500ms per burst   */
bool MincConsumeWalkMarker(char *reasonOut, size_t cap); /* read + DELETE; false when absent */
