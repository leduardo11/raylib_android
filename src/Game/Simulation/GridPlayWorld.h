#pragma once

#include "Game/Input/TargetVerb.h"
#include "Game/Simulation/GridCoord.h"
#include "Game/Simulation/GridWorld.h"
#include "Game/Simulation/TargetWorld.h"

#include <cstdint>
#include <vector>

// GridPlayWorld: the app-screen ITargetWorld adapter. Tile validity comes from
// the sim's GridWorld; the player tile is refreshed each frame; the entity set
// is a small demo list (monster/item/NPC) that the HUD's tap-to-target and the
// greedy nav observe. No execution here, only observation.

namespace Simulation {

class GridPlayWorld final : public ITargetWorld {
public:
    void setGrid(GridWorld* grid) { m_grid = grid; }
    void setPlayer(GridCoord p) { m_player = p; }
    void setPlayerId(uint32_t id) { m_playerId = id; }
    void add(const TargetInfo& t) { m_entities.push_back(t); }

    GridCoord playerPosition() const override { return m_player; }
    bool isPlayerTargetId(uint32_t id) const override { return id == m_playerId; }

    bool tryGetTarget(uint32_t id, TargetInfo& out) const override
    {
        for (const auto& e : m_entities)
            if (e.id == id) { out = e; return true; }
        return false;
    }

    bool findTargetAt(const GridCoord& tile, TargetInfo& out) const override
    {
        for (const auto& e : m_entities)
            if (e.position.x == tile.x && e.position.y == tile.y)
            {
                out = e;
                return true;
            }
        return false;
    }

    int interactionRange(Input::TargetVerb verb, const TargetInfo&) const override
    {
        if (verb == Input::TargetVerb::Move) return 0;
        return m_range[static_cast<int>(verb)];
    }

    bool isTileValid(const GridCoord& tile) const override
    {
        return m_grid && m_grid->canStepTo(tile);
    }

private:
    GridWorld* m_grid = nullptr;
    GridCoord m_player;
    uint32_t m_playerId = 1;
    std::vector<TargetInfo> m_entities;
    int m_range[4] = { 0, 1, 1, 1 }; // Move/Attack/Pickup/Interact
};

} // namespace Simulation