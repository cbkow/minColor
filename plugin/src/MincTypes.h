/* AE-free types shared by the engine and the plugin shell (keeps OcioEngine buildable
   without the After Effects SDK — used by tools/probe-engine and, later, tests).      */
#pragma once
#include <cstdint>

#define MINC_ARB_MAGIC   0x4D6E4341u
#define MINC_ARB_VERSION 1
#define MINC_SPACE_LEN   200

enum MinColorDirection { MINC_DIR_TO_WORKING = 0, MINC_DIR_FROM_WORKING = 1 };

typedef struct {
    uint32_t magic;                     /* fixed-width: this struct IS the flat serialized form */
    uint16_t version;
    uint16_t direction;
    uint32_t instanceId;                /* assigned at SEQUENCE_SETUP; survives every AE clone of the data */
    char     space[MINC_SPACE_LEN];
} MinColorArb;

enum MincStatus {
    MINC_STATUS_OK = 0,
    MINC_STATUS_PASS_EMPTY,
    MINC_STATUS_PASS_OCIO_OFF,
    MINC_STATUS_PASS_UNKNOWN_SPACE,
    MINC_STATUS_PASS_CONFIG_ERROR
};

typedef struct {
    bool  ocioOn;
    char  configPath[1024];
    char  workingSpace[MINC_SPACE_LEN];
    uint32_t generation;
} MincAuthoritySnapshot;

typedef struct {                       /* handed to instances via AEGP_EffectCallGeneric */
    uint32_t    magic;                 /* MINC_ARB_MAGIC */
    MinColorArb arb;
} MincSyncPayload;

/* Authority.cpp: instance registry — AE clones render-side sequence data from stale snapshots,
   so mutable state flows through this map keyed by the instanceId baked into the data. */
void MincRegistrySet(uint32_t id, const MinColorArb *arb);
bool MincRegistryGet(uint32_t id, MinColorArb *out);

int MincOcioProbeStatus(const MincAuthoritySnapshot *auth, const MinColorArb *arb);  /* ladder check, no pixels */
int MincOcioApplyRows(const MincAuthoritySnapshot *auth, const MinColorArb *arb,
                      float *rgbaRows, int pixelCount);
