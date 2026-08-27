/* AE-free types shared by the engine and the plugin shell (keeps OcioEngine buildable
   without the After Effects SDK — used by tools/probe-engine and, later, tests).      */
#pragma once

#define MINC_ARB_MAGIC   0x4D6E4341u
#define MINC_ARB_VERSION 1
#define MINC_SPACE_LEN   200

enum MinColorDirection { MINC_DIR_TO_WORKING = 0, MINC_DIR_FROM_WORKING = 1 };

typedef struct {
    unsigned long  magic;
    unsigned short version;
    unsigned short direction;
    char           space[MINC_SPACE_LEN];
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
    unsigned long generation;
} MincAuthoritySnapshot;

int MincOcioApplyRows(const MincAuthoritySnapshot *auth, const MinColorArb *arb,
                      float *rgbaRows, int pixelCount);
