#include "GridWorld.h"

namespace Simulation {

GridWorld::GridWorld() = default;

GridWorld::GridWorld(int width, int height)
{
    setSize(width, height);
}

void GridWorld::setSize(int width, int height)
{
    m_width  = width;
    m_height = height;
    m_walkable.assign(width * height, false);
}

void GridWorld::setWalkable(int x, int y, bool walkable)
{
    if (isInBounds(x, y))
        m_walkable[y * m_width + x] = walkable;
}

bool GridWorld::isInBounds(int x, int y) const
{
    return x >= 0 && x < m_width && y >= 0 && y < m_height;
}

bool GridWorld::isInBounds(const GridCoord& c) const
{
    return isInBounds(c.x, c.y);
}

bool GridWorld::isWalkable(int x, int y) const
{
    if (!isInBounds(x, y)) return false;
    return m_walkable[y * m_width + x];
}

bool GridWorld::isWalkable(const GridCoord& c) const
{
    return isWalkable(c.x, c.y);
}

bool GridWorld::canStepTo(const GridCoord& c) const
{
    return isInBounds(c) && isWalkable(c);
}

void GridWorld::markSimpleMap()
{
    for (int y = 0; y < m_height; ++y)
        for (int x = 0; x < m_width; ++x)
            setWalkable(x, y, true);

    auto block = [this](int x, int y) { setWalkable(x, y, false); };

    for (int i = 0; i < m_width; ++i)  { block(i, 0); block(i, m_height - 1); }
    for (int j = 0; j < m_height; ++j) { block(0, j); block(m_width - 1, j); }

    for (int i = 7; i <= 9; ++i)
        for (int j = 5; j <= 7; ++j)
            block(i, j);

    for (int i = 20; i <= 22; ++i)
        for (int j = 8; j <= 10; ++j)
            block(i, j);

    for (int i = 12; i <= 16; ++i) block(i, 14);
    for (int i = 4;  i <= 7;  ++i) block(i, 3);
}

} // namespace Simulation
