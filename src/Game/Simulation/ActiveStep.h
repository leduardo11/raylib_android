#pragma once

#include "GridCoord.h"
#include "HelbreathDirection.h"
#include "MovementTiming.h"

namespace Simulation {

struct ActiveStep {
    GridCoord origin;
    GridCoord destination;
    Direction direction = Direction::S;
    Locomotion locomotion = Locomotion::Walking;
    float durationMs    = 0.0f;
    float elapsedMs     = 0.0f;
    bool  active        = false;

    bool isComplete() const { return elapsedMs >= durationMs; }

    float progress() const
    {
        if (durationMs <= 0.0f) return 1.0f;
        float t = elapsedMs / durationMs;
        return (t > 1.0f) ? 1.0f : t;
    }
};

} // namespace Simulation
