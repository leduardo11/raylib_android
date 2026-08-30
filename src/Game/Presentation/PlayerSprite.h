#pragma once

#include "Game/Content/AtlasV2.h"
#include "Game/Content/TexturePack.h"
#include "Game/Simulation/HelbreathDirection.h"
#include "Game/Presentation/PlayerPresentationState.h"

#include <string>

namespace Presentation {

// Animated player rendered from an atlas@2 package (cs_rpg paperdoll-sheet
// baseline). Read-only: consumes an interpolated PlayerPresentationState and
// never learns about committed steps or grid authority.
class PlayerSprite {
public:
    bool load(const std::string& pkgPath);
    bool loaded() const { return m_loaded; }

    void update(const PlayerPresentationState& s, float dtMs);
    void draw(const PlayerPresentationState& s, float tileSize);

private:
    const Content::AnimClip* resolveClip(const PlayerPresentationState& s);

    Content::AtlasV2Package m_pkg;
    Content::TexturePack m_textures;
    std::string m_clipId;
    float m_elapsedMs = 0.0f;
    bool m_loaded = false;
};

} // namespace Presentation