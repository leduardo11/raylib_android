#pragma once

#include "AtlasV2.h"

#include <string>
#include <vector>

namespace Content {

// The cs_rpg / HelbreathAtlasPacker map runtime package: a tilemap grid plus
// the map texture atlas and a collision grid. Parsing is pure; texture and
// GPU lifetime belong to the renderer.
struct TileMapData {
    int width = 0;
    int height = 0;
    int tileSize = 32;

    // Region id per tile (width * height), index 0 = empty placeholder.
    std::vector<int> ground;

    // Walkability from the collision package: cell == 0 is walkable.
    std::vector<bool> walkable;

    // Spawn tile, from manifest.json playerSpawn.
    int spawnTileX = 0;
    int spawnTileY = 0;

    // Texture atlas (atlas.json): file names relative to the atlas dir.
    std::vector<std::string> textures;
    std::vector<AtlasRegion> regions;
};

// Parses the four map-package JSON documents. Throws std::runtime_error on
// malformed or structurally incompatible input.
TileMapData parseTileMap(const std::string& tilemapJson,
                         const std::string& atlasJson,
                         const std::string& collisionJson,
                         const std::string& manifestJson);

} // namespace Content