#pragma once

#include "Game/Simulation/HelbreathDirection.h"

#include <cmath>

namespace Input {

// Converts an input device vector (or angle) into a canonical Helbreath
// direction. This is input-device interpretation ONLY — it maps the physical
// direction a finger/stick points at to the simulation's 8-way model. The
// simulation itself knows nothing about vectors, atan2, or dead zones.
struct DirectionQuantizer {
    static Simulation::Direction fromVector(float x, float y);
    static Simulation::Direction fromAngle(float radians);
};

inline Simulation::Direction DirectionQuantizer::fromAngle(float radians)
{
    // Screen coordinates (y grows downward). A unit vector at angle 0 points
    // east; angles increase clockwise through SE (45), S (90), SW (135),
    // W (180), NW (225), N (270), NE (315).
    //
    // Sector map (sectors are the eight equal 45-degree wedges):
    //   0 -> E(3), 1 -> SE(4), 2 -> S(5), 3 -> SW(6),
    //   4 -> W(7), 5 -> NW(8), 6 -> N(1), 7 -> NE(2)
    constexpr float TwoPi      = 2.0f * 3.14159265358979323846f;
    constexpr float SectorSize = TwoPi / 8.0f;
    constexpr float HalfSector = SectorSize / 2.0f;

    float a = radians;
    if (a < 0.0f) a += TwoPi;

    int sector = static_cast<int>((a + HalfSector) / SectorSize) % 8;
    int value  = ((sector + 2) % 8) + 1;

    return static_cast<Simulation::Direction>(value);
}

inline Simulation::Direction DirectionQuantizer::fromVector(float x, float y)
{
    if (x == 0.0f && y == 0.0f)
        return Simulation::Direction::S;

    return fromAngle(atan2f(y, x));
}

} // namespace Input