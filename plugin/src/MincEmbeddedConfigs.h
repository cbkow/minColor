/* Embedded OCIO configs + their LUTs, baked into the EFFECT binary (zlib-compressed) so the
   effect renders with ZERO filesystem dependency — "the plugin is the package." The .cpp is
   GENERATED at build time (plugin/cmake/gen_embedded_configs.py) from config/dist. Lookup is by
   BASENAME (all LUT basenames are unique across the icc/luts/filmic trees). Every accessor returns
   empty when the name isn't embedded, so the caller falls back to the filesystem store (custom or
   legacy configs). The AEGP still reads configs from disk (it runs interactively with the plugin
   installed); only the effect's render path is embedded. */
#pragma once
#include <string>
#include <vector>
#include <cstdint>

std::string          MincEmbeddedConfig(const std::string &basename);   /* inflated config YAML ("" if none) */
std::vector<uint8_t> MincEmbeddedLut(const std::string &basename);      /* inflated LUT bytes (empty if none) */
bool                 MincEmbeddedHasConfig(const std::string &basename);
