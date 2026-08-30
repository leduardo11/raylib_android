#pragma once

#include "GridCoord.h"

#include <cstdint>

namespace Simulation {

enum class Direction : uint8_t {
    N  = 1,
    NE = 2,
    E  = 3,
    SE = 4,
    S  = 5,
    SW = 6,
    W  = 7,
    NW = 8,
    COUNT = 8
};

inline Offset directionOffset(Direction d)
{
    switch (d)
    {
        case Direction::N:  return { 0, -1 };
        case Direction::NE: return { 1, -1 };
        case Direction::E:  return { 1,  0 };
        case Direction::SE: return { 1,  1 };
        case Direction::S:  return { 0,  1 };
        case Direction::SW: return { -1, 1 };
        case Direction::W:  return { -1, 0 };
        case Direction::NW: return { -1, -1 };
        default:            return { 0,  0 };
    }
}

inline bool isDirectionDiagonal(Direction d)
{
    auto off = directionOffset(d);
    return off.x != 0 && off.y != 0;
}

} // namespace Simulation