#pragma once

#include "Game/Simulation/GridCoord.h"
#include "Game/Simulation/HelbreathDirection.h"
#include "Game/Simulation/MovementTiming.h"
#include "TargetVerb.h"

#include <cstddef>
#include <cstdint>
#include <variant>

// PlayerCommand: the small, semantic boundary between *input producers*
// (joystick, keyboard/mouse, touch HUD, network) and everything downstream
// (sim/nav, protocol translation).
//
// Rules of this file:
//  - No raylib, no I/O, no wire/packet types. Pure intent.
//  - Commands are packed structs so producers stay allocation-free.
//  - Keep the command set small. New verbs need discussion, not a habit.
//
// The six kinds are fixed, per the approved v2 proposal:
//   Move / SetTarget / Attack / Cast / UseItem / ToggleStack
//
// SetTarget carries the explicit Verb — the resolver never guesses intent
// from target type. Momentary hold-to-run is resolved at the producer edge:
// a held RUN button makes the next Move use Locomotion::Running directly.

namespace Input {

// Joystick / arrow-key movement. Standing = stop in place.
struct PlayerMove {
    Simulation::Direction direction = Simulation::Direction::S;
    Simulation::Locomotion locomotion = Simulation::Locomotion::Walking;
};

// "Go do {verb} with this." TargetResolver consumes this; the greedy
// navigator resolves position -> steps. For entity targets the producer can
// fill targetId directly; otherwise TargetResolver finds the entity at
// `position`.
struct PlayerSetTarget {
    Simulation::GridCoord position;
    uint32_t targetId = 0;
    bool hasTargetId = false; // false => resolver must find entity at `position`
    TargetVerb verb = TargetVerb::Move;
};

// Direct combat command: hit this entity with a normal (or SUPER) attack.
// Action type / swing gating / distance are resolved downstream, not here.
struct PlayerAttack {
    uint32_t targetId = 0;
    bool super = false; // SUPER modifier pressed next to this attack
};

enum class CastKind : uint8_t {
    Spell       = 0, // from magic window / F4 / magic shortcut
    SpecAbility = 1  // PageUp parity: RequestActivateSpecAbility
};

struct PlayerCast {
    uint16_t magicId = 0;
    CastKind kind = CastKind::Spell;
};

// Consumable / shortcut slots. Which wire op (ReqUseItem vs short-cut magic)
// and the bound item/spell are resolved by the CommandTranslator against the
// slot-binding registry — PlayerCommand stays intent-only.
enum class UseItemSlot : uint8_t {
    Shortcut1 = 0, // F2 parity (touch slot 1)
    Shortcut2 = 1, // F3 parity (touch slot 2)
    Shortcut3 = 2, // F4 cast slot (touch slot 3)
    Hp        = 3, // Insert parity
    Mp        = 4  // Delete parity
};

struct PlayerUseItem {
    UseItemSlot slot = UseItemSlot::Shortcut1;
};

// Mode switches with visible state. `on` is the *target state*:
//   - momentary RUN button: {Run, true} on press, {Run, false} on release
//     (sets locomotion for subsequent moves AND nav-generated moves).
//   - persistent Ctrl+R parity: {Run, <new state>} once per toggle.
//   - Window: `windowId` selects the dialog; `on` opens/closes it.
enum class ToggleKind : uint8_t {
    Run         = 0, // locomotion: run vs walk
    Stance      = 1, // Tab parity:    combat stance on/off
    SafeAttack  = 2, // Home parity:   safe-attack mode
    ForceAttack = 3, // Ctrl+A parity: force attack (no target requirement)
    Window      = 4  // F5..F10 parity (UI-only dialog, no wire op)
};

struct PlayerToggle {
    ToggleKind kind = ToggleKind::Run;
    bool on = true;
    uint16_t windowId = 0; // only when kind == Window
};

// The six-command boundary, enforced as a closed variant.
using PlayerCommand = std::variant<
    PlayerMove,
    PlayerSetTarget,
    PlayerAttack,
    PlayerCast,
    PlayerUseItem,
    PlayerToggle>;

inline constexpr size_t playerCommandKindCount = std::variant_size_v<PlayerCommand>;

} // namespace Input