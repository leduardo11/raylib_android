#pragma once

#include "HelbreathDirection.h"
#include "MovementTiming.h"

namespace Simulation {

struct MoveIntent {
    Direction direction = Direction::S;
    Locomotion locomotion = Locomotion::Standing;
    bool active = false;
};

} // namespace Simulation
