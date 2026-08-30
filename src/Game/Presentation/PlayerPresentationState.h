#pragma once

#include "Game/Simulation/HelbreathDirection.h"
#include "Game/Simulation/MovementTiming.h"
#include "raylib.h"

namespace Presentation {

struct PlayerPresentationState {
    Vector2  position   = { 0, 0 };
    Simulation::Direction facing    = Simulation::Direction::S;
    Simulation::Locomotion locomotion = Simulation::Locomotion::Standing;
    float    stepProgress = 0.0f;
    bool     isMoving     = false;
    float    animTimer    = 0.0f;
};

inline void advanceAnimTimer(PlayerPresentationState& s, float dt)
{
    if (s.isMoving)
        s.animTimer += dt;
    else
        s.animTimer = 0.0f;
}

} // namespace Presentation