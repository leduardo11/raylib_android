#pragma once

#include "Game/Input/PlayerCommand.h"
#include "Game/Simulation/TargetWorld.h"

#include <optional>

// TargetResolver: decides WHAT and WHERE the player must act on, given a raw
// SetTarget request and the current world. It is deliberately NOT a pathfinder:
// it has no notion of steps, directions, adjacency, or blocked-retry. Those all
// belong to GreedyNavigator (item 4). It can only see the world, not change it.

namespace Simulation {

struct ResolvedTarget {
    uint32_t targetId = 0;
    bool hasTargetId = false;
    GridCoord destination;
    Input::TargetVerb verb = Input::TargetVerb::Move;
    int approachRange = 0; // stop when within this many tiles of destination
};

// Resolution rules (each request → exactly one answer):
//   - Ground Move: destination = the tapped tile; requires a valid tile.
//   - Entity verbs (Attack/Pickup/Interact): the entity must exist — found by
//     targetId if given, else at the tapped tile — and must carry the requested
//     capability. destination = the entity's CURRENT position (fresh anchor, so
//     moving targets don't lag); approachRange comes from the world.
//   - Any entity verb that cannot be honored (missing entity, wrong capability)
//     falls back to Move toward the tapped tile — "do the best thing with this
//     target" parity. That fallback also requires a valid tile.
//   - Targeting the player themselves is not resolvable → nullopt (no-op).
class TargetResolver {
public:
    static std::optional<ResolvedTarget> resolve(const ITargetWorld& world,
                                                 const Input::PlayerSetTarget& req);
};

inline std::optional<ResolvedTarget> TargetResolver::resolve(
    const ITargetWorld& world,
    const Input::PlayerSetTarget& req)
{
    // Ground move: only valid tiles are targetable.
    if (req.verb == Input::TargetVerb::Move)
    {
        if (!world.isTileValid(req.position)) return std::nullopt;
        return ResolvedTarget{ 0, false, req.position, Input::TargetVerb::Move, 0 };
    }

    // Entity verb: locate the entity first.
    TargetInfo ent;
    const bool found = req.hasTargetId
        ? world.tryGetTarget(req.targetId, ent)
        : world.findTargetAt(req.position, ent);

    // Never resolve an action against the player themselves: strict no-op.
    if (found && world.isPlayerTargetId(ent.id)) return std::nullopt;

    const bool capable =
        found &&
        ((req.verb == Input::TargetVerb::Attack && ent.attackable) ||
         (req.verb == Input::TargetVerb::Pickup && ent.pickupable) ||
         (req.verb == Input::TargetVerb::Interact && ent.interactable));

    if (capable)
    {
        return ResolvedTarget{
            ent.id,
            true,
            ent.position, // fresh anchor: follow moving targets
            req.verb,
            world.interactionRange(req.verb, ent)
        };
    }

    // Fallback: nothing actionable at that spot — walk up to it like a
    // left-click on empty ground. Same validity gate as ground Move.
    if (!world.isTileValid(req.position)) return std::nullopt;
    return ResolvedTarget{ 0, false, req.position, Input::TargetVerb::Move, 0 };
}

} // namespace Simulation