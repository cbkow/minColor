#include "AEConfig.h"
#include "AE_EffectVers.h"
#include "MincIds.h"

#ifndef AE_OS_WIN
    #include <AE_General.r>
#endif

/* The four variants whose MATCH NAME carries the verb and whose display name carries the
   space, plus — WINDOWS ONLY — legacy "MINC CST" (name-grammar verbs). Mac retired legacy
   registration in M3 step 8: 1.x instances open as benign placeholders (RESULTS §35) and
   Migrate resurrects them as variants. Windows keeps legacy until the M4 AEGP brings the
   ceremonies there. All effects share GlobalSetup, so the OutFlags hexes are identical.
   Entry point strings must match the exported symbols byte-for-byte (extern "C").
   Variants build on BOTH platforms (the M3 Windows session proves the PiPLtool multi-resource path).          */

#ifdef AE_OS_WIN
resource 'PiPL' (16000) {
    {
        Kind { AEEffect },
        Name { MINC_NAME_LEGACY },
        Category { MINC_CATEGORY },
        CodeWin64X86 { MINC_ENTRY_LEGACY },
        AE_PiPL_Version { 2, 0 },
        AE_Effect_Spec_Version { PF_PLUG_IN_VERSION, PF_PLUG_IN_SUBVERS },
        AE_Effect_Version { MINC_PIPL_VERSION_HEX },
        AE_Effect_Info_Flags { 0 },
        AE_Effect_Global_OutFlags  { MINC_PIPL_OUTFLAGS },
        AE_Effect_Global_OutFlags_2 { MINC_PIPL_OUTFLAGS2 },
        AE_Effect_Match_Name { MINC_MATCH_LEGACY },
        AE_Reserved_Info { 0 },
        AE_Effect_Support_URL { "https://github.com/" }
    }
};
#endif  /* legacy is Windows-only from M3 step 8 */

resource 'PiPL' (16001) {
    {
        Kind { AEEffect },
        Name { MINC_NAME_XFORM },
        Category { MINC_CATEGORY },
#ifdef AE_OS_WIN
        CodeWin64X86 { MINC_ENTRY_XFORM },
#else
        CodeMacARM64 { MINC_ENTRY_XFORM },
        CodeMacIntel64 { MINC_ENTRY_XFORM },
#endif
        AE_PiPL_Version { 2, 0 },
        AE_Effect_Spec_Version { PF_PLUG_IN_VERSION, PF_PLUG_IN_SUBVERS },
        AE_Effect_Version { MINC_PIPL_VERSION_HEX },
        AE_Effect_Info_Flags { 0 },
        AE_Effect_Global_OutFlags  { MINC_PIPL_OUTFLAGS },
        AE_Effect_Global_OutFlags_2 { MINC_PIPL_OUTFLAGS2 },
        AE_Effect_Match_Name { MINC_MATCH_XFORM },
        AE_Reserved_Info { 0 },
        AE_Effect_Support_URL { "https://github.com/" }
    }
};

resource 'PiPL' (16002) {
    {
        Kind { AEEffect },
        Name { MINC_NAME_VIEW },
        Category { MINC_CATEGORY },
#ifdef AE_OS_WIN
        CodeWin64X86 { MINC_ENTRY_VIEW },
#else
        CodeMacARM64 { MINC_ENTRY_VIEW },
        CodeMacIntel64 { MINC_ENTRY_VIEW },
#endif
        AE_PiPL_Version { 2, 0 },
        AE_Effect_Spec_Version { PF_PLUG_IN_VERSION, PF_PLUG_IN_SUBVERS },
        AE_Effect_Version { MINC_PIPL_VERSION_HEX },
        AE_Effect_Info_Flags { 0 },
        AE_Effect_Global_OutFlags  { MINC_PIPL_OUTFLAGS },
        AE_Effect_Global_OutFlags_2 { MINC_PIPL_OUTFLAGS2 },
        AE_Effect_Match_Name { MINC_MATCH_VIEW },
        AE_Reserved_Info { 0 },
        AE_Effect_Support_URL { "https://github.com/" }
    }
};

resource 'PiPL' (16003) {
    {
        Kind { AEEffect },
        Name { MINC_NAME_RENDER },
        Category { MINC_CATEGORY },
#ifdef AE_OS_WIN
        CodeWin64X86 { MINC_ENTRY_RENDER },
#else
        CodeMacARM64 { MINC_ENTRY_RENDER },
        CodeMacIntel64 { MINC_ENTRY_RENDER },
#endif
        AE_PiPL_Version { 2, 0 },
        AE_Effect_Spec_Version { PF_PLUG_IN_VERSION, PF_PLUG_IN_SUBVERS },
        AE_Effect_Version { MINC_PIPL_VERSION_HEX },
        AE_Effect_Info_Flags { 0 },
        AE_Effect_Global_OutFlags  { MINC_PIPL_OUTFLAGS },
        AE_Effect_Global_OutFlags_2 { MINC_PIPL_OUTFLAGS2 },
        AE_Effect_Match_Name { MINC_MATCH_RENDER },
        AE_Reserved_Info { 0 },
        AE_Effect_Support_URL { "https://github.com/" }
    }
};

resource 'PiPL' (16004) {
    {
        Kind { AEEffect },
        Name { MINC_NAME_LOOK },
        Category { MINC_CATEGORY },
#ifdef AE_OS_WIN
        CodeWin64X86 { MINC_ENTRY_LOOK },
#else
        CodeMacARM64 { MINC_ENTRY_LOOK },
        CodeMacIntel64 { MINC_ENTRY_LOOK },
#endif
        AE_PiPL_Version { 2, 0 },
        AE_Effect_Spec_Version { PF_PLUG_IN_VERSION, PF_PLUG_IN_SUBVERS },
        AE_Effect_Version { MINC_PIPL_VERSION_HEX },
        AE_Effect_Info_Flags { 0 },
        AE_Effect_Global_OutFlags  { MINC_PIPL_OUTFLAGS },
        AE_Effect_Global_OutFlags_2 { MINC_PIPL_OUTFLAGS2 },
        AE_Effect_Match_Name { MINC_MATCH_LOOK },
        AE_Reserved_Info { 0 },
        AE_Effect_Support_URL { "https://github.com/" }
    }
};
