#pragma once

#include <cstdint>
#include <variant>

// ProtocolCommand: the protocol-level *intent* layer between the game boundary
// (Input::PlayerCommand) and the wire (HelbreathPacketEncoder, item 6).
//
// These types mirror the reference client's packet builders (PacketSendHelpers:
// make_motion / make_motion_attack / make_common_command) but hold only what a
// command means on the wire — never raw bytes, never ENet framing, never
// message ids or timestamps. The encoder owns all of that.
//
// Numeric enums are pinned to hb_lite's ActionID.h / NetMessages.h values so
// the encoder and any integration testing stay byte-stable.

namespace Protocol {

// Wire Type:: ids — hb_lite ActionID.h.
enum class ActionType : uint8_t {
    Stop        = 0,
    Move        = 1,
    Run         = 2,
    Attack      = 3,
    Magic       = 4,
    GetItem     = 5,
    Damage      = 6,
    DamageMove  = 7,
    AttackMove  = 8,
    Dying       = 10,
    NullAction  = 100
};

// Wire CommonType:: ids — hb_lite NetMessages.h (subset used by the boundary).
enum class CommonType : uint16_t {
    EquipItem                 = 0x0A02,
    ReleaseItem               = 0x0A0A,
    ToggleCombatMode          = 0x0A0B,
    Magic                     = 0x0A0D,
    ReqUseItem                = 0x0A11,
    ToggleSafeAttackMode      = 0x0A18,
    TalkToNpc                 = 0x0A1A,
    RequestActivateSpecAbility = 0x0A40
};

// PacketCommandMotionSimple: stop / move / run / getitem / magic.
struct Motion {
    ActionType action = ActionType::Stop;
    uint8_t dir = 0;        // 1..8 (Helbreath direction ids); 0 = keep facing
    int16_t x = 0;          // player position at send time
    int16_t y = 0;
    int16_t dx = 0;         // deltas; dx = magic id when action == Magic
    int16_t dy = 0;
    int16_t type = 0;       // unused for simple motion
};

// PacketCommandMotionAttack: Attack / AttackMove.
struct MotionAttack {
    ActionType action = ActionType::Attack;
    uint8_t dir = 0;            // facing toward the target
    int16_t x = 0;              // player position at send time
    int16_t y = 0;
    int16_t destX = 0;          // target tile
    int16_t destY = 0;
    int16_t weaponActionType = 0; // strike style, from equipment (ctx)
    uint16_t targetId = 0;
};

// PacketCommandCommon: toggles, spec ability, use-item / equip.
struct Common {
    CommonType cmd = CommonType::ToggleCombatMode;
    int16_t x = 0;
    int16_t y = 0;
    uint8_t dir = 0;
    int16_t slot = -1;      // inventory slot for ReqUseItem / EquipItem (-1 = none)
    int16_t idx = 0;        // secondary index fields (window cells); unused today
};

using ProtocolCommand = std::variant<Motion, MotionAttack, Common>;

} // namespace Protocol