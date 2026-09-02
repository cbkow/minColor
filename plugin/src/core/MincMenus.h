/* plugin-menus.json reader — CORE (both binaries): the christening walk (AEGP) and the badge
   menu (effect, M2 step 5) read the SAME file the AEGP writes from presets.json. mtime+size
   cached; unreadable/incomplete file -> valid=false and callers SKIP (christening never
   invents a space).                                                                        */
#pragma once
#include "MincTypes.h"

#define MINC_MENU_MAX 128

const char *MincSharedSettingsDir(void);        /* /Users/Shared/minColor/settings (win: ProgramData) */

typedef struct {
    bool valid;
    char preset[128];
    char family[32];
    char defaultView[MINC_SPACE_LEN];
    char defaultRender[MINC_SPACE_LEN];
    int  nInput, nView, nRender, nLooks;
    char inputSpaces[MINC_MENU_MAX][MINC_SPACE_LEN];
    char viewSpaces[MINC_MENU_MAX][MINC_SPACE_LEN];
    char renderSpaces[MINC_MENU_MAX][MINC_SPACE_LEN];
    char looks[MINC_MENU_MAX][MINC_SPACE_LEN];
} MincMenus;

bool MincMenusGet(MincMenus *out);              /* false when absent/unreadable/no defaults */
