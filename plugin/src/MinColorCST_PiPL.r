#include "AEConfig.h"
#include "AE_EffectVers.h"

#ifndef AE_OS_WIN
    #include <AE_General.r>
#endif

resource 'PiPL' (16000) {
    {
        Kind { AEEffect },
        Name { "minColor CST" },
        Category { "minColor" },
#ifdef AE_OS_WIN
        CodeWin64X86 { "EffectMain" },
#else
        CodeMacARM64 { "EffectMain" },
        CodeMacIntel64 { "EffectMain" },
#endif
        AE_PiPL_Version { 2, 0 },
        AE_Effect_Spec_Version { PF_PLUG_IN_VERSION, PF_PLUG_IN_SUBVERS },
        AE_Effect_Version { 0x00080001 },   /* 1.0, develop; keep in sync with PF_VERSION */
        AE_Effect_Info_Flags { 0 },
        AE_Effect_Global_OutFlags  { 0x06008410 },
        AE_Effect_Global_OutFlags_2 { 0x08201400 },
        AE_Effect_Match_Name { "MINC CST" },
        AE_Reserved_Info { 0 },
        AE_Effect_Support_URL { "https://github.com/" }
    }
};
