/* Embedded TEXT metadata, baked into the AEGP binary so the authoring side (panel dropdowns,
   looks, interpret) works with ZERO config store on disk — "the plugin is the package" for the
   AEGP too, matching the effect's embedded configs+LUTs. Holds presets.json, extension-defaults.json
   and the preset config-*.ocio (YAML text, NO LUTs — the effect carries those). Stored raw so the
   mac AEGP needs no zlib link. The .cpp is GENERATED at build time (plugin/cmake/gen_embedded_meta.py)
   from config/. Every reader goes disk-FIRST and falls back here, so a real config store still wins
   (custom/edited files) and the embed is the safety net when the store isn't installed. Lookup by
   basename. */
#pragma once
#include <string>

std::string MincEmbeddedMetaText(const std::string &basename);   /* file text ("" if not embedded) */
bool        MincEmbeddedMetaHas(const std::string &basename);
