/* Standalone harness: exercises MincOcioApplyRows against a real sidecar config.
   Usage: probe-engine <config.ocio> <working-space> <direction:to|from> <space> r g b [r g b ...] */
#include "../src/MincTypes.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <vector>
int main(int argc, char **argv) {
    if (argc < 8 || ((argc - 5) % 3) != 0) { fprintf(stderr, "usage: %s cfg ws to|from space r g b...\n", argv[0]); return 2; }
    MincAuthoritySnapshot auth = {}; auth.ocioOn = true; auth.generation = 1;
    snprintf(auth.configPath, sizeof(auth.configPath), "%s", argv[1]);
    snprintf(auth.workingSpace, sizeof(auth.workingSpace), "%s", argv[2]);
    MinColorArb arb = {}; arb.magic = MINC_ARB_MAGIC; arb.version = MINC_ARB_VERSION;
    arb.direction = strcmp(argv[3], "to") == 0 ? MINC_DIR_TO_WORKING : MINC_DIR_FROM_WORKING;
    snprintf(arb.space, sizeof(arb.space), "%s", argv[4]);
    int n = (argc - 5) / 3;
    std::vector<float> px((size_t)n * 4, 1.0f);
    for (int i = 0; i < n; ++i)
        for (int c = 0; c < 3; ++c) px[(size_t)i*4+c] = (float)atof(argv[5 + i*3 + c]);
    int st = MincOcioApplyRows(&auth, &arb, px.data(), n);
    printf("status=%d\n", st);
    for (int i = 0; i < n; ++i) printf("%.8f %.8f %.8f\n", px[(size_t)i*4], px[(size_t)i*4+1], px[(size_t)i*4+2]);
    return st == MINC_STATUS_OK ? 0 : 1;
}
