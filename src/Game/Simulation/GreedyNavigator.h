#pragma once

#include "Game/Simulation/GridCoord.h"
#include "Game/Simulation/HelbreathDirection.h"
#include "Game/Simulation/TargetResolver.h"
#include "Game/Simulation/TargetWorld.h"

#include <optional>

// GreedyNavigator: decides the SINGLE next step toward a ResolvedTarget using
// exact Helbreath-style greedy motion — NOT a pathfinder.
//
//   - Re-evaluated every committed step: `next(world, from)` takes the player's
//     CURRENT tile and computes the next move from scratch. No precomputed
//     path, no path queue (A* is deferred).
//   - Greedy bias: prefer the diagonal toward the target; when both axes
//     remain, consume the larger axis first as a detour (blocked-retry).
//   - Reached when within approachRange on both axes (the same |dx|<=range &&
//     |dy|<=range rule the reference client uses for strike range).
//   - Blocked = the ideal step was walled off but a valid detour step is taken
//     (reticle: red — moving, but not on the clean path).
//   - Stuck = no forward step exists at all; the only open neighbor is the tile
//     we came from (which is never retraced). Retracing is the caller's choice.
//   - World is read-only observation via ITargetWorld.

namespace Simulation {

class GreedyNavigator {
public:
    enum class Status : uint8_t {
        Idle = 0,    // no target engaged
        Moving = 1,  // emitted the ideal greedy step
        Reached = 2, // within approachRange of destination
        Blocked = 3, // ideal blocked; emitted a detour step
        Stuck = 4    // no forward step available
    };

    struct Result {
        Status status = Status::Idle;
        Direction direction = Direction::S; // valid when Moving | Blocked
    };

    void setTarget(const ResolvedTarget& target)
    {
        m_target = target;
        m_prevTile.reset();
    }

    void clear()
    {
        m_target.reset();
        m_prevTile.reset();
    }

    bool hasTarget() const { return m_target.has_value(); }
    const ResolvedTarget* target() const { return m_target ? &*m_target : nullptr; }

    Result next(const ITargetWorld& world, GridCoord from);

private:
    std::optional<ResolvedTarget> m_target;
    std::optional<GridCoord> m_prevTile; // tile stepped FROM last; never retraced
};

inline GreedyNavigator::Result GreedyNavigator::next(const ITargetWorld& world,
                                                     GridCoord from)
{
    constexpr Direction NONE = Direction::COUNT;
    if (!m_target) return { Status::Idle, Direction::S };

    const ResolvedTarget& t = *m_target;
    const int dxr = t.destination.x - from.x;
    const int dyr = t.destination.y - from.y;
    const int adx = (dxr < 0) ? -dxr : dxr;
    const int ady = (dyr < 0) ? -dyr : dyr;

    // Reach gate: same |dx|<=range && |dy|<=range rule as the reference
    // client's strike range. For ground Move (range 0) this means exact tile.
    if (adx <= t.approachRange && ady <= t.approachRange)
        return { Status::Reached, Direction::S };

    const int sx = (dxr > 0) ? 1 : (dxr < 0) ? -1 : 0;
    const int sy = (dyr > 0) ? 1 : (dyr < 0) ? -1 : 0;

    // Ideal greedy step: diagonal toward the target, else along the remaining axis.
    Direction ideal = NONE;
    if (sx != 0 && sy != 0)
        ideal = (sx > 0 && sy > 0) ? Direction::SE
              : (sx > 0 && sy < 0) ? Direction::NE
              : (sx < 0 && sy > 0) ? Direction::SW
                                   : Direction::NW;
    else if (sx != 0)
        ideal = (sx > 0) ? Direction::E : Direction::W;
    else
        ideal = (sy > 0) ? Direction::S : Direction::N;

    // Detour candidates: the two axes, larger residual offset attempted first.
    Direction axA = NONE, axB = NONE;
    if (adx >= ady)
    {
        if (sx != 0) axA = (sx > 0) ? Direction::E : Direction::W;
        if (sy != 0) axB = (sy > 0) ? Direction::S : Direction::N;
    }
    else
    {
        if (sy != 0) axA = (sy > 0) ? Direction::S : Direction::N;
        if (sx != 0) axB = (sx > 0) ? Direction::E : Direction::W;
    }

    const Direction candidates[3] = { ideal, axA, axB };
    for (int i = 0; i < 3; ++i)
    {
        const Direction d = candidates[i];
        if (d == NONE) continue;

        const GridCoord tile = from + directionOffset(d);
        if (!world.isTileValid(tile)) continue;
        if (m_prevTile && tile.x == m_prevTile->x && tile.y == m_prevTile->y)
            continue; // never emit an immediate back-track

        m_prevTile = from;
        return { d == ideal ? Status::Moving : Status::Blocked, d };
    }

    // No forward step: boxed in. The only open tile would be where we came from.
    return { Status::Stuck, Direction::S };
}

} // namespace Simulation