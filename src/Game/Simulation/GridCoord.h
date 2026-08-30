#pragma once

#include <cstdint>

namespace Simulation {

struct Offset { int x; int y; };

struct GridCoord {
    int x = 0;
    int y = 0;

    bool operator==(const GridCoord& o) const { return x == o.x && y == o.y; }
    bool operator!=(const GridCoord& o) const { return !(*this == o); }

    GridCoord operator+(const GridCoord& o) const { return { x + o.x, y + o.y }; }
    GridCoord operator+(const Offset& o) const { return { x + o.x, y + o.y }; }
};

struct GridBounds {
    int left   = 0;
    int top    = 0;
    int width  = 0;
    int height = 0;

    bool contains(int x, int y) const
    {
        return x >= left && x < left + width &&
               y >= top  && y < top  + height;
    }

    bool containsCoord(const GridCoord& c) const
    {
        return contains(c.x, c.y);
    }
};

struct GridSize {
    int width  = 0;
    int height = 0;
};

} // namespace Simulation
