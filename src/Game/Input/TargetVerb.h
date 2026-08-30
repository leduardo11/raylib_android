#pragma once

#include <cstdint>

// TargetVerb: the cross-cutting, intent-level verb that a SetTarget command
// means. Lives on its own so both the input boundary (PlayerCommand) and the
// world-observation seam (ITargetWorld) can share it without depending on each
// other's full type sets.

namespace Input {

// The verb a SetTarget command wants done with the target. One tap in,
// one unambiguous action out.
enum class TargetVerb : uint8_t {
    Move     = 0, // walk/run to the tile (ground target)
    Attack   = 1, // approach within range, then Attack
    Pickup   = 2, // approach, then GetItem
    Interact = 3  // approach, then the chosen NPC interaction
};

} // namespace Input