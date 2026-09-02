/* presets.json access for the ceremonies — minColorAEGP only. Discovery order: the central
   MediaCore store (the plugin's own), then /Users/Shared/minColor/configs (both verified on
   disk; both generated from the same source). Logged which was used.                       */
#pragma once
#include <string>

struct MincPresetInfo {
    bool        valid = false;
    bool        retired = false;
    std::string key, config, working, family, label, workingSpaceLabel, pwcsJSON;
};

std::string   MincCentralConfigsDir(void);               /* MediaCore/minColor/configs */
std::string   MincPresetsFileUsed(void);                 /* the presets.json actually loaded ("" if none) */
MincPresetInfo MincPresetMeta(const std::string &key);   /* live OR retired record */
std::string   MincFamilyFor(const std::string &key);     /* "Linear" | "Display" (default Linear) */

/* pin identity from the hashed filename: "config-<preset>-<hex>.ocio" -> preset key.
   Returns "" when the basename doesn't match CONFIG_PATTERN.                              */
std::string MincPresetFromConfigBase(const std::string &base);

/* current config for a preset vs a pinned basename -> "behind" when they differ */
bool MincPinBehind(const std::string &presetKey, const std::string &pinnedBase,
                   std::string *pinnedOut, std::string *currentOut);

/* filename-match search: central store, project sidecar (_minColor beside projPath), shared configs */
std::string MincFindConfigByName(const std::string &base, const std::string &projPathOrEmpty);
