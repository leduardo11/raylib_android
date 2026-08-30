#pragma once

#include "AtlasV2.h"
#include "raylib.h"

#include <string>
#include <vector>

namespace Content {

// A rectangular piece of an atlas region as it lives inside one chunk
// texture after fixed-grid slicing. `ox/oy` are the piece's offset from the
// top-left of the ORIGINAL region, so renderers can blit pieces back
// together at destination + (ox,oy): a region that straddles a 2048 cut
// becomes 2-4 pieces that tile seamlessly.
struct GpuPiece {
    int texture = 0;
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
    int ox = 0;
    int oy = 0;
};

struct TexturePack {
    std::vector<Texture2D> textures;             // chunk GPU textures (flattened)
    std::vector<std::vector<GpuPiece>> pieces;   // per ORIGINAL region id
};

inline constexpr int MAX_TEXTURE_DIM = 2048;

// Fixed-grid cut positions along one axis: 0, maxDim, 2*maxDim, ... clipped
// to sheetDim. Never pushes beyond maxDim — chunks that would land on a
// region are handled by splitPieces instead. Pure.
std::vector<int> gridCuts(int sheetDim, int maxDim);

// Clips every region of `textureIndex` against the chunk grid so no piece
// exceeds maxDim in either axis. A region touching two cells per axis yields
// up to 4 pieces, each carrying its offset within the original region.
// `ncols` is the chunk column count of this sheet and `chunkBase` the
// flattened texture-slot offset of this sheet's first chunk.
std::vector<std::vector<GpuPiece>>
buildPieces(const std::vector<AtlasRegion>& regions, int textureIndex,
            const std::vector<int>& xs, const std::vector<int>& ys,
            int ncols, int chunkBase);

// Loads each sheet file, slices it into <= MAX_TEXTURE_DIM chunks and
// uploads one texture per chunk. out.pieces is filled for every region of
// every sheet (indexed by original region id). Returns false on any failure.
bool loadTexturePack(const char* dir,
                     const std::vector<std::string>& files,
                     const std::vector<AtlasRegion>& inRegions,
                     TexturePack& out);

void unloadTexturePack(TexturePack& pack);

} // namespace Content