#pragma once

#include "Game/Input/TargetVerb.h"
#include "Game/Simulation/GridCoord.h"

#include <array>
#include <cstdint>
#include <vector>

// ITargetWorld: the *world-observation* seam for targeting and navigation.
//
// It answers ONLY the questions TargetResolver / GreedyNavigator need:
//   - Does an entity exist?            tryGetTarget
//   - Where is it?                     TargetInfo::position
//   - What kind of target is it?       TargetInfo::kind
//   - Is it attack/pickup/interactable?TargetInfo::attackable/pickupable/interactable
//   - Where is the player?             playerPosition / isPlayerTargetId
//   - Is a tile valid to step on?      isTileValid
//
// It NEVER executes anything: no attack, no pickup, no movement, no verb
// resolution. Semantic decisions (raw target -> SetTarget{position, id, verb})
// belong to the TargetResolver (next slice).

namespace Simulation {

enum class TargetKind : uint8_t {
    Monster = 0, // attackable
    Item    = 1, // pickup-able
    Npc     = 2  // interactable
};

struct TargetInfo {
    uint32_t id = 0;
    GridCoord position;
    TargetKind kind = TargetKind::Npc;
    bool attackable = false;
    bool pickupable = false;
    bool interactable = false;
};

class ITargetWorld {
public:
    virtual ~ITargetWorld() = default;

    // Player state ------------------------------------------------------
    virtual GridCoord playerPosition() const = 0;
    virtual bool isPlayerTargetId(uint32_t id) const = 0;

    // Entity observation -----------------------------------------------
    // Existence + full info for a known id.
    virtual bool tryGetTarget(uint32_t id, TargetInfo& out) const = 0;
    // First entity at a tile (insertion order for ties): deterministic.
    virtual bool findTargetAt(const GridCoord& tile, TargetInfo& out) const = 0;

    // Rules (data, not behavior) ---------------------------------------
    // How close (in tiles) the player must be to act with this verb.
    virtual int interactionRange(Input::TargetVerb verb,
                                 const TargetInfo& target) const = 0;
    // Tile step-able: in bounds + not a wall. Entities never permanently
    // block tiles (the player walks onto an enemy tile to close distance).
    virtual bool isTileValid(const GridCoord& tile) const = 0;
};

// Deterministic, test-oriented stub. Simple injected shapes: a walkable
// rectangle, optional blocked tiles, and a fixed entity list. Later the real
// world is a NetworkMapDataAdapter over CMapData snapshots.
class StubTargetWorld final : public ITargetWorld {
public:
    StubTargetWorld(int width, int height, GridCoord player = { 0, 0 })
        : m_width(width)
        , m_height(height)
        , m_walkable(static_cast<size_t>(width) * static_cast<size_t>(height), true)
        , m_player(player)
    {
    }

    // Injection helpers ------------------------------------------------
    StubTargetWorld& setPlayer(GridCoord p) { m_player = p; return *this; }
    StubTargetWorld& setPlayerId(uint32_t id) { m_playerId = id; return *this; }
    StubTargetWorld& add(const TargetInfo& t) { m_entities.push_back(t); return *this; }
    StubTargetWorld& setWalkable(int x, int y, bool walkable)
    {
        if (inBounds(x, y)) m_walkable[idx(x, y)] = walkable;
        return *this;
    }
    StubTargetWorld& setRange(Input::TargetVerb verb, int range)
    {
        if (verb != Input::TargetVerb::Move) m_range[static_cast<int>(verb)] = range;
        return *this;
    }

    // ITargetWorld ------------------------------------------------------
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

    int interactionRange(Input::TargetVerb verb,
                         const TargetInfo& /*target*/) const override
    {
        return m_range[static_cast<int>(verb)];
    }

    bool isTileValid(const GridCoord& tile) const override
    {
        return inBounds(tile.x, tile.y) && m_walkable[idx(tile.x, tile.y)];
    }

private:
    int m_width  = 0;
    int m_height = 0;
    bool inBounds(int x, int y) const
    {
        return x >= 0 && x < m_width && y >= 0 && y < m_height;
    }
    size_t idx(int x, int y) const
    {
        return static_cast<size_t>(y) * m_width + static_cast<size_t>(x);
    }

    std::vector<bool> m_walkable;
    std::vector<TargetInfo> m_entities;
    GridCoord m_player;
    uint32_t m_playerId = 1;

    // index = static_cast<int>(Input::TargetVerb); Move(unused)=0.
    std::array<int, 4> m_range = { 0, 1 /*Attack*/, 1 /*Pickup*/, 1 /*Interact*/ };
};

} // namespace Simulation