#pragma once

#include "Game/Simulation/HelbreathDirection.h"
#include "Game/Simulation/MoveIntent.h"
#include "Game/Simulation/MovementTiming.h"
#include "DirectionQuantizer.h"
#include "raylib.h"
#include <cmath>

namespace Input {

// Reads WASD / Arrow keys and produces a normalized direction vector.
// Returns false when no movement key is held.
inline bool readKeyboardVector(float& outDX, float& outDY)
{
    bool up    = IsKeyDown(KEY_W) || IsKeyDown(KEY_UP);
    bool down  = IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN);
    bool left  = IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT);
    bool right = IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT);

    if (!up && !down && !left && !right)
    {
        outDX = 0.0f;
        outDY = 0.0f;
        return false;
    }

    float dx = (right ? 1.0f : 0.0f) - (left ? 1.0f : 0.0f);
    float dy = (down  ? 1.0f : 0.0f) - (up   ? 1.0f : 0.0f);
    float len = sqrtf(dx * dx + dy * dy);
    outDX = dx / len;
    outDY = dy / len;
    return true;
}

// Converts a normalized direction vector into a Run MoveIntent.
inline Simulation::MoveIntent keyboardMoveIntent(float dx, float dy)
{
    Simulation::MoveIntent intent;
    intent.direction  = DirectionQuantizer::fromVector(dx, dy);
    intent.locomotion = Simulation::Locomotion::Running;
    intent.active     = true;
    return intent;
}

} // namespace Input