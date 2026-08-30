#pragma once

#include "AtlasV2.h"

#include "raylib.h"

#include <string>
#include <vector>

namespace Content {

// GLES2-safe maximum texture dimension. The map sheet (4096x4079) and player
// sheet (8175x204) exceed plain device limits, so sheets are split into
// chunks of at most this size at load time, before any GL upload.
constexpr int MAX_TEXTURE_DIM = 2048;

struct TexturePack {
    std::vector<Texture2D> textures;   // chunk textures, owned by the pack
    std::vector<AtlasRegion> regions;  // remapped: texture->chunk index, x/y within chunk (same order/count as input)
};

// Computes split positions along one axis of a sheet so that no region ever
// straddles a cut. `horizontal` selects the x extents (true) or y extents
// (false). Pure (shared by the loader and the unit tests).
std::vector<int> splitPoints(const std::vector<AtlasRegion>& regions,
                             int textureIndex, int sheetDim, int maxDim,
                             bool horizontal);

// Loads every sheet in `files` (under `dir/`), splits each into
// MAX_TEXTURE_DIM chunks and remaps `inRegions` into `out`. The empty
// placeholder region (id 0) is preserved untouched. Returns false (and
// unloads what it already loaded) when any sheet file cannot be loaded.
bool loadTexturePack(const char* dir,
                     const std::vector<std::string>& files,
                     const std::vector<AtlasRegion>& inRegions,
                     TexturePack& out);

void unloadTexturePack(TexturePack& pack);

} // namespace Content