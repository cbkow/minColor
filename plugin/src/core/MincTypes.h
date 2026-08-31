/* AE-free types shared by the engine and the plugin shell (keeps OcioEngine buildable
   without the After Effects SDK — used by tools/probe-engine and, later, tests).      */
#pragma once
#include <cstdint>
#include <cstring>
#include "MincIds.h"                    /* identity strings shared with the PiPLs (pure #defines) */

#define MINC_ARB_MAGIC   0x4D6E4341u
#define MINC_ARB_VERSION 1
#define MINC_SPACE_LEN   200

enum MinColorDirection { MINC_DIR_TO_WORKING = 0, MINC_DIR_FROM_WORKING = 1, MINC_DIR_LOOK = 2 };

/* which registered effect an instance came in through. NOT serialized — AE's stored match
   name is the durable form; this enum is just its in-process shape (entry-point wrappers
   pass it statically). LEGACY = the pre-2.0 all-verbs effect, verb lives in the name.    */
enum MincVerb { MINC_VERB_XFORM = 0, MINC_VERB_VIEW, MINC_VERB_RENDER, MINC_VERB_LOOK, MINC_VERB_LEGACY };

/* match-name recognition — the P4 contract: match name = verb authority. Fills the verb
   (LEGACY for the pre-2.0 effect) and returns true for any of the five registered minColor
   effects; false for foreign match names.                                                */
static inline bool MincMatchVerb(const char *match, MincVerb *out) {
    if (!strcmp(match, MINC_MATCH_XFORM))  { *out = MINC_VERB_XFORM;  return true; }
    if (!strcmp(match, MINC_MATCH_VIEW))   { *out = MINC_VERB_VIEW;   return true; }
    if (!strcmp(match, MINC_MATCH_RENDER)) { *out = MINC_VERB_RENDER; return true; }
    if (!strcmp(match, MINC_MATCH_LOOK))   { *out = MINC_VERB_LOOK;   return true; }
    if (!strcmp(match, MINC_MATCH_LEGACY)) { *out = MINC_VERB_LEGACY; return true; }
    return false;
}
static inline bool MincIsOurs(const char *match) { MincVerb v; return MincMatchVerb(match, &v); }
/* does a display-name kind token agree with the match-name verb? (legacy: name is the
   authority, everything agrees). A contradiction is NEVER silently reinterpreted.        */
/* kind token -> the variant that owns it (inverse of MincKindMatchesVerb; input + contain
   live on the Transform effect) — the authored dialect from M3 on                        */
static inline const char *MincMatchForKind(const char *kind) {
    if (!strcmp(kind, "view"))   return MINC_MATCH_VIEW;
    if (!strcmp(kind, "render")) return MINC_MATCH_RENDER;
    if (!strcmp(kind, "look"))   return MINC_MATCH_LOOK;
    return MINC_MATCH_XFORM;
}
static inline bool MincKindMatchesVerb(const char *kind, MincVerb v) {
    switch (v) {
        case MINC_VERB_LEGACY: return true;
        case MINC_VERB_XFORM:  return !strcmp(kind, "input") || !strcmp(kind, "contain");
        case MINC_VERB_VIEW:   return !strcmp(kind, "view");
        case MINC_VERB_RENDER: return !strcmp(kind, "render");
        default:               return !strcmp(kind, "look");
    }
}

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
/* badge-menu fallback enumeration (effect binary only — OCIO lives there) */
int MincOcioListSpaces(const MincAuthoritySnapshot *auth, char out[][MINC_SPACE_LEN], int maxN);
int MincOcioListLooks(const MincAuthoritySnapshot *auth, char out[][MINC_SPACE_LEN], int maxN);
