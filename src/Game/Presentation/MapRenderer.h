#pragma once

#include "Game/Content/TileMap.h"
#include "Game/Content/TexturePack.h"
#include "Game/Presentation/Camera.h"

#include <string>

namespace Presentation {

// Read-only world view: owns the map package textures and blits only the
// visible tile range. Never mutates simulation state.
class MapRenderer {
public:
    // Loads the map package from `dir` (tilemap/atlas/collision/manifest jsons
    // and atlases/ subfolder). Returns false without touching the screen when
    // any file is missing.
    bool load(const std::string& dir);

    const Content::TileMapData& data() const { return m_data; }
    bool loaded() const { return m_loaded; }

    // Blits the visible ground tiles for the given camera.
    void draw(const Camera& cam) const;

private:
    Content::TileMapData m_data;
    Content::TexturePack m_textures;
    bool m_loaded = false;
};

} // namespace Presentation