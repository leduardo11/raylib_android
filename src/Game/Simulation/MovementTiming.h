#pragma once

#include <cstdint>

namespace Simulation {

constexpr float WALK_DURATION_MS = 560.0f;
constexpr float RUN_DURATION_MS  = 312.0f;

enum class Locomotion : uint8_t {
    Standing,
    Walking,
    Running
};

} // namespace Simulation
