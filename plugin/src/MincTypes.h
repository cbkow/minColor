/* AE-free types shared by the engine and the plugin shell (keeps OcioEngine buildable
   without the After Effects SDK — used by tools/probe-engine and, later, tests).      */
#pragma once
#include <cstdint>

#define MINC_ARB_MAGIC   0x4D6E4341u
#define MINC_ARB_VERSION 1
#define MINC_SPACE_LEN   200

enum MinColorDirection { MINC_DIR_TO_WORKING = 0, MINC_DIR_FROM_WORKING = 1, MINC_DIR_LOOK = 2 };

typedef struct {
    uint32_t magic;                     /* fixed-width: this struct IS the flat serialized form */
    uint16_t version;
    uint16_t direction;
    uint32_t instanceId;                /* assigned at SEQUENCE_SETUP; survives every AE clone of the data */
    char     space[MINC_SPACE_LEN];
} MinColorArb;

#define MINC_SEQ_VERSION    2
#define MINC_CONFIGBASE_LEN 128

typedef struct {                        /* v2 SEQUENCE data (the arb PARAM stays plain MinColorArb).
                                           First member IS the v1 seq struct, so old plugins reading the
                                           212-byte prefix stay correct. The passport (configBase +
                                           passportWorking) travels inside the .aep and lets the render
                                           resolve the content-addressed config from the LOCAL store when
                                           live authority is dead (cross-platform arrival, aerender). */
    MinColorArb arb;
    uint16_t    seqVersion;             /* MINC_SEQ_VERSION */
    uint16_t    reserved;
    char        configBase[MINC_CONFIGBASE_LEN];   /* hashed config FILENAME, e.g. config-acescg-84aa8926.ocio */
    char        passportWorking[MINC_SPACE_LEN];   /* working space at last healthy sync */
} MincSeqData;

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

typedef struct {                       /* handed to instances via AEGP_EffectCallGeneric.
                                          v2 fields APPENDED after arb — the {magic, arb} prefix
                                          stays v1-compatible (sender and receiver are always the
                                          same binary; the check is belt-and-braces). */
    uint32_t    magic;                 /* MINC_ARB_MAGIC */
    MinColorArb arb;
    uint16_t    payVersion;            /* 3: + outId/newId */
    uint16_t    reserved2;
    char        configBase[MINC_CONFIGBASE_LEN];   /* empty when authority is sick at sync time */
    char        passportWorking[MINC_SPACE_LEN];
    uint32_t    outId;                 /* v3, WRITTEN BY THE RECEIVER: the instance's id after the call */
    uint32_t    newId;                 /* v3, sender: non-zero = adopt this id (duplicate repair) */
} MincSyncPayload;

/* Instance ids: unique across sessions (random per-session seed, then monotonic). A process-global
   counter starting at 1 minted duplicates across sessions; the registry is keyed by id, so two
   effects sharing one executed each other's transform (found 2026-08-29 on an SDR project). */
uint32_t MincMintInstanceId(void);

/* Authority.cpp: instance registry — AE clones render-side sequence data from stale snapshots,
   so mutable state flows through this map keyed by the instanceId baked into the data.
   Stores the WHOLE seq struct so registry and seq clone are interchangeable at render. */
void MincRegistrySet(uint32_t id, const MincSeqData *sd);
bool MincRegistryGet(uint32_t id, MincSeqData *out);

/* Passport.cpp — self-locating store + effective authority (see file header) */
const char *MincLocalStoreConfigDir(void);
bool MincEffectiveAuthority(const MincAuthoritySnapshot *live, const MincSeqData *sd,
                            MincAuthoritySnapshot *out);   /* true == synthesized from passport */

int MincOcioProbeStatus(const MincAuthoritySnapshot *auth, const MinColorArb *arb);  /* ladder check, no pixels */
int MincOcioApplyRows(const MincAuthoritySnapshot *auth, const MinColorArb *arb,
                      float *rgbaRows, int pixelCount);                /* one-shot (probe tool) */
int  MincOcioBegin(const MincAuthoritySnapshot *auth, const MinColorArb *arb, void **token);  /* per-frame */
void MincOcioApplyToken(void *token, float *rgbaRows, int pixelCount);
void MincOcioEnd(void *token);
