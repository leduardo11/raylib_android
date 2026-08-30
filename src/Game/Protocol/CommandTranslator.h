#pragma once

#include "Game/Input/PlayerCommand.h"
#include "Game/Protocol/ProtocolCommand.h"
#include "Game/Simulation/TargetWorld.h"

#include <cstdint>
#include <vector>

// CommandTranslator: Input::PlayerCommand (game intent) → Protocol::ProtocolCommand
// (wire intent). The UI/sim never touch ProtocolCommand directly; the encoder
// (item 6) turns ProtocolCommand into bytes.
//
// The translator stays a pure mapping. Everything it needs beyond the command
// itself is *observation* passed in explicitly:
//   - ITargetWorld  — player position, target position (spatial facts)
//   - IWireContext  — equipment action types + inventory slot resolution
// No state, no side effects, no emitting, no world mutation.

namespace Protocol {

// One shortcut slot's binding: either an inventory item slot (equip) or a
// magic index (cast).
struct ShortcutBinding {
    bool isValid = false;
    bool isMagic = false;
    int16_t itemSlot = 0;  // inventory slot for EquipItem
    uint16_t magicId = 0;  // magic id for Motion{Magic}
};

// Equipment/inventory facts the pure command text cannot carry.
class IWireContext {
public:
    virtual ~IWireContext() = default;

    // Strike style id for a normal/super attack (from equipped weapon).
    virtual int16_t attackActionType(bool super) const = 0;

    // Inventory slot holding the Hp/Mp consumable; -1 when none available.
    virtual int16_t consumableSlot(Input::UseItemSlot slot) const = 0;

    // Shortcut slot 0..2 (Shortcut1/2/3). Returns false when unbound.
    virtual bool shortcutBinding(uint8_t shortcutSlot, ShortcutBinding& out) const = 0;
};

// Helbreath facing id (Direction N=1..NW=8) from a->b; 0 when aligned.
inline uint8_t facingToward(Simulation::GridCoord from, Simulation::GridCoord to)
{
    const int dx = to.x - from.x;
    const int dy = to.y - from.y;
    if (dx == 0 && dy == 0) return 0;
    const int sx = (dx > 0) ? 1 : -1;
    const int sy = (dy > 0) ? 1 : -1;
    return
        (sx > 0 && sy > 0) ? 4  // SE
      : (sx > 0 && sy < 0) ? 2  // NE
      : (sx < 0 && sy > 0) ? 6  // SW
      : (sx < 0 && sy < 0) ? 8  // NW
      : (sx > 0)           ? 3  // E
      : (sx < 0)           ? 7  // W
      : (sy > 0)           ? 5  // S
                           : 1; // N
}

class CommandTranslator {
public:
    static std::vector<ProtocolCommand> translate(
        const Input::PlayerCommand& cmd,
        const Simulation::ITargetWorld& world,
        const IWireContext& ctx);
};

inline std::vector<ProtocolCommand> CommandTranslator::translate(
    const Input::PlayerCommand& cmd,
    const Simulation::ITargetWorld& world,
    const IWireContext& ctx)
{
    std::vector<ProtocolCommand> out;

    if (const auto* mv = std::get_if<Input::PlayerMove>(&cmd))
    {
        const Simulation::GridCoord p = world.playerPosition();
        Protocol::ActionType action = Protocol::ActionType::Stop;
        if (mv->locomotion == Simulation::Locomotion::Running)
            action = Protocol::ActionType::Run;
        else if (mv->locomotion == Simulation::Locomotion::Walking)
            action = Protocol::ActionType::Move;
        out.emplace_back(Protocol::Motion{
            action,
            static_cast<uint8_t>(mv->direction), // 1..8 wire direction ids
            static_cast<int16_t>(p.x), static_cast<int16_t>(p.y),
            0, 0, 0 });
        return out;
    }

    if (const auto* atk = std::get_if<Input::PlayerAttack>(&cmd))
    {
        Simulation::TargetInfo target;
        if (!world.tryGetTarget(atk->targetId, target)) return out;   // stale id: drop
        if (world.isPlayerTargetId(atk->targetId)) return out;        // never self-hit
        const Simulation::GridCoord p = world.playerPosition();
        out.emplace_back(Protocol::MotionAttack{
            Protocol::ActionType::Attack,
            facingToward(p, target.position),
            static_cast<int16_t>(p.x), static_cast<int16_t>(p.y),
            static_cast<int16_t>(target.position.x),
            static_cast<int16_t>(target.position.y),
            ctx.attackActionType(atk->super),
            static_cast<uint16_t>(atk->targetId) });
        return out;
    }

    if (const auto* cast = std::get_if<Input::PlayerCast>(&cmd))
    {
        const Simulation::GridCoord p = world.playerPosition();
        if (cast->kind == Input::CastKind::Spell)
        {
            out.emplace_back(Protocol::Motion{
                Protocol::ActionType::Magic,
                0,
                static_cast<int16_t>(p.x), static_cast<int16_t>(p.y),
                static_cast<int16_t>(cast->magicId), 0, 0 }); // dx = magic id
        }
        else
        {
            out.emplace_back(Protocol::Common{
                Protocol::CommonType::RequestActivateSpecAbility,
                static_cast<int16_t>(p.x), static_cast<int16_t>(p.y), 0, -1, 0 });
        }
        return out;
    }

    if (const auto* use = std::get_if<Input::PlayerUseItem>(&cmd))
    {
        const Simulation::GridCoord p = world.playerPosition();
        if (use->slot == Input::UseItemSlot::Hp ||
            use->slot == Input::UseItemSlot::Mp)
        {
            const int16_t slot = ctx.consumableSlot(use->slot);
            if (slot < 0) return out; // no potion available: drop
            out.emplace_back(Protocol::Common{
                Protocol::CommonType::ReqUseItem,
                static_cast<int16_t>(p.x), static_cast<int16_t>(p.y), 0, slot, 0 });
            return out;
        }
        ShortcutBinding binding;
        const uint8_t idx = static_cast<uint8_t>(use->slot); // Shortcut1..3 = 0..2
        if (!ctx.shortcutBinding(idx, binding)) return out;  // unbound: drop
        if (binding.isMagic)
        {
            out.emplace_back(Protocol::Motion{
                Protocol::ActionType::Magic,
                0,
                static_cast<int16_t>(p.x), static_cast<int16_t>(p.y),
                static_cast<int16_t>(binding.magicId), 0, 0 });
        }
        else
        {
            out.emplace_back(Protocol::Common{
                Protocol::CommonType::EquipItem,
                static_cast<int16_t>(p.x), static_cast<int16_t>(p.y), 0,
                binding.itemSlot, 0 });
        }
        return out;
    }

    if (const auto* tg = std::get_if<Input::PlayerToggle>(&cmd))
    {
        using Protocol::CommonType;
        const Simulation::GridCoord p = world.playerPosition();
        CommonType type = CommonType::ToggleCombatMode;
        bool emits = true;
        switch (tg->kind)
        {
            case Input::ToggleKind::Stance:     type = CommonType::ToggleCombatMode; break;
            case Input::ToggleKind::SafeAttack: type = CommonType::ToggleSafeAttackMode; break;
            default:
                // Run modulates locomotion of subsequent moves (no packet);
                // ForceAttack/Window are purely local.
                emits = false;
                break;
        }
        if (emits)
            out.emplace_back(Protocol::Common{
                type, static_cast<int16_t>(p.x), static_cast<int16_t>(p.y), 0, -1, 0 });
        return out;
    }

    // PlayerSetTarget: no direct wire op. It drives navigation; the resulting
    // Move / Attack commands carry the traffic. Nothing to emit.
    return out;
}

} // namespace Protocol