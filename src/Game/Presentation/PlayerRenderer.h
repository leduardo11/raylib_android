#pragma once

#include "Game/Simulation/GridCoord.h"
#include "Game/Simulation/HelbreathDirection.h"
#include "PlayerPresentationState.h"
#include "raylib.h"

namespace Presentation {

constexpr float KEEP_PLAYER_AROUND = 3.0f;

// Read-only presentation of the player. Receives interpolated state only; it
// never reasons about committed steps or grid authority.
class PlayerRenderer {
public:
    void draw(const PlayerPresentationState& s, float tileSize);

private:
    void drawPlayerShape(const PlayerPresentationState& s, float tileSize);
    void drawDirectionArrow(Vector2 center, Simulation::Direction facing,
                            float tileSize);
    void drawGlowRing(const PlayerPresentationState& s, float tileSize);
};

} // namespace Presentation