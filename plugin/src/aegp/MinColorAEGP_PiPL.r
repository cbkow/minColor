#include "AEConfig.h"

#ifndef AE_OS_WIN
    #include "AE_General.r"
#endif

/* minColorAEGP — startup ceremonies bundle. Loader dispatch is by CFBundlePackageType (AEgx,
   in InfoAEGP.plist.in) + this Kind{AEGP} PiPL; lives in the app Plug-ins folder.           */
resource 'PiPL' (16000) {
    {
        Kind { AEGP },
        Name { "minColor AEGP" },
        Category { "General Plugin" },
        Version { 196608 },
#ifdef AE_OS_WIN
        CodeWin64X86 { "MincAegpEntry" },
#else
        CodeMacARM64 { "MincAegpEntry" },
        CodeMacIntel64 { "MincAegpEntry" },
#endif
    }
};
