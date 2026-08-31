#include "AEConfig.h"
#include "AE_EffectVers.h"
#include "MincIds.h"

#ifndef AE_OS_WIN
    #include <AE_General.r>
#endif

/* Five effects, one binary (2.0): legacy "MINC CST" (all verbs, name grammar — registered
   through M2, retires in M3) plus four variants whose MATCH NAME carries the verb and whose
   display name carries the space. All five share GlobalSetup, so the OutFlags hexes are
   identical. Entry point strings must match the exported symbols byte-for-byte (extern "C").
   Variants are mac-only until M4 proves the Windows PiPLtool multi-resource path.          */

resource 'PiPL' (16000) {
    {
        Kind { AEEffect },
        Name { MINC_NAME_LEGACY },
        Category { MINC_CATEGORY },
#ifdef AE_OS_WIN
        CodeWin64X86 { MINC_ENTRY_LEGACY },
#else
        CodeMacARM64 { MINC_ENTRY_LEGACY },
        CodeMacIntel64 { MINC_ENTRY_LEGACY },
#endif
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

#ifndef AE_OS_WIN

resource 'PiPL' (16001) {
    {
        Kind { AEEffect },
        Name { MINC_NAME_XFORM },
        Category { MINC_CATEGORY },
        CodeMacARM64 { MINC_ENTRY_XFORM },
        CodeMacIntel64 { MINC_ENTRY_XFORM },
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
        CodeMacARM64 { MINC_ENTRY_VIEW },
        CodeMacIntel64 { MINC_ENTRY_VIEW },
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
        CodeMacARM64 { MINC_ENTRY_RENDER },
        CodeMacIntel64 { MINC_ENTRY_RENDER },
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
        CodeMacARM64 { MINC_ENTRY_LOOK },
        CodeMacIntel64 { MINC_ENTRY_LOOK },
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

#endif  /* variants mac-only until M4 */
