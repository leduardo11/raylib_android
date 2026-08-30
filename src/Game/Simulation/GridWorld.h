#pragma once

#include <vector>
#include "GridCoord.h"

namespace Simulation {

class GridWorld {
public:
    GridWorld();
    GridWorld(int width, int height);

    void setSize(int width, int height);
    void setWalkable(int x, int y, bool walkable);

    bool isInBounds(int x, int y) const;
    bool isInBounds(const GridCoord& c) const;
    bool isWalkable(int x, int y) const;
    bool isWalkable(const GridCoord& c) const;
    bool canStepTo(const GridCoord& c) const;

    int width()  const { return m_width; }
    int height() const { return m_height; }

    void markSimpleMap();

private:
    int m_width  = 0;
    int m_height = 0;
    std::vector<bool> m_walkable;
};

} // namespace Simulation
