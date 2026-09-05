/* Windows preset picker — the portable tiers of MincPicker.mm without the mac NSAlert dialog.
   The panel always writes shell-args before invoking a command, so the dialog is only reached by
   a MENU-invoked command with no panel; on Windows that path is a no-op (returns false — the
   panel is the way). CMake selects this .cpp on WIN32 and MincPicker.mm on APPLE. */
#include "MincPicker.h"
#include "MincCore.h"
#include "MincArgs.h"
#include "MincSettings.h"
#include "MincPresets.h"
#include "MincJson.h"

bool MincPickPreset(const char *commandLabel, std::string *keyOut) {
    {   /* shell-args first: the shell's dropdown already made the choice */
        MincJsonPtr a = MincArgsConsume(commandLabel);
        if (a) {
            std::string k = a->str("preset");
            if (!k.empty() && MincPresetMeta(k).valid) { *keyOut = k; return true; }
            MincLog("picker: shell-args preset invalid ('%s') — falling through", k.c_str());
        }
    }
    {   /* quiet-answers (automation) */
        MincJsonPtr qa = MincJsonParseFile(MincSettingsDir() + "/quiet-answers.json");
        if (qa) {
            std::string k = qa->str("preset");
            if (!k.empty() && MincPresetMeta(k).valid) { *keyOut = k; return true; }
        }
    }
    MincLog("picker: no shell-args/quiet-answers preset (Windows has no dialog fallback)");
    return false;
}
