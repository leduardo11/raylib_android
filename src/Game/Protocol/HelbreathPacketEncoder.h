#pragma once

#include "Game/Protocol/ProtocolCommand.h"

#include <cstddef>
#include <cstdint>
#include <vector>

// HelbreathPacketEncoder: dumb, deterministic ProtocolCommand → wire bytes.
//
// Discipline of this boundary:
//   - Every non-envelope byte comes 1:1 from the command. No reinterpreting,
//     no target resolution, no gameplay state, no invented fields.
//   - The only additions are the transport envelope the reference client's
//     builders always fill: the per-packet msg_id constant and the caller-
//     supplied time_ms (session clock). msg ids are serialization constants;
//     time comes from EncodeContext, never guessed.
//   - Serialization is explicit little-endian (the wire format), so host
//     endianness is irrelevant.
//
// Layouts (packed, little-endian), matching hb_lite
// src/shared/includes/Packet/PacketRequest.h:
//
//   Motion        -> PacketCommandMotionSimple  (21 bytes)
//     msg_id(4) msg_type(2) x(2) y(2) dir(1) dx(2) dy(2) type(2) time(4)
//   MotionAttack  -> PacketCommandMotionAttack  (23 bytes)
//     msg_id(4) msg_type(2) x(2) y(2) dir(1) dx(2) dy(2) type(2) target_id(2) time(4)
//   Common        -> PacketCommandCommonWithTime (27 bytes)
//     msg_id(4) msg_type(2) x(2) y(2) dir(1) v1(4) v2(4) v3(4) time(4)

namespace Protocol {

inline constexpr uint32_t kMsgIdMotion = 0x0FA314D5u; // MsgId::CommandMotion
inline constexpr uint32_t kMsgIdCommon = 0x0FA314DCu; // MsgId::CommandCommon

struct EncodeContext {
    uint32_t timeMs = 0; // session clock, supplied by the caller at send time
};

class HelbreathPacketEncoder {
public:
    static size_t encodedSize(const ProtocolCommand& cmd);

    static std::vector<uint8_t> encode(const ProtocolCommand& cmd,
                                       const EncodeContext& ctx);
};

inline size_t HelbreathPacketEncoder::encodedSize(const ProtocolCommand& cmd)
{
    if (std::holds_alternative<Motion>(cmd)) return 21;
    if (std::holds_alternative<MotionAttack>(cmd)) return 23;
    return 27; // Common
}

namespace detail {
inline void writeLE(std::vector<uint8_t>& out, uint64_t v, size_t bytes)
{
    for (size_t i = 0; i < bytes; ++i)
        out.push_back(static_cast<uint8_t>(v >> (8 * i)));
}
} // namespace detail

inline std::vector<uint8_t> HelbreathPacketEncoder::encode(
    const ProtocolCommand& cmd, const EncodeContext& ctx)
{
    std::vector<uint8_t> out;
    out.reserve(encodedSize(cmd));

    if (const auto* m = std::get_if<Motion>(&cmd))
    {
        detail::writeLE(out, kMsgIdMotion, 4);
        detail::writeLE(out, static_cast<uint16_t>(m->action), 2);
        detail::writeLE(out, static_cast<uint16_t>(m->x), 2);
        detail::writeLE(out, static_cast<uint16_t>(m->y), 2);
        out.push_back(m->dir);
        detail::writeLE(out, static_cast<uint16_t>(m->dx), 2);
        detail::writeLE(out, static_cast<uint16_t>(m->dy), 2);
        detail::writeLE(out, static_cast<uint16_t>(m->type), 2);
        detail::writeLE(out, ctx.timeMs, 4);
    }
    else if (const auto* a = std::get_if<MotionAttack>(&cmd))
    {
        detail::writeLE(out, kMsgIdMotion, 4);
        detail::writeLE(out, static_cast<uint16_t>(a->action), 2);
        detail::writeLE(out, static_cast<uint16_t>(a->x), 2);
        detail::writeLE(out, static_cast<uint16_t>(a->y), 2);
        out.push_back(a->dir);
        detail::writeLE(out, static_cast<uint16_t>(a->destX), 2);
        detail::writeLE(out, static_cast<uint16_t>(a->destY), 2);
        detail::writeLE(out, static_cast<uint16_t>(a->weaponActionType), 2);
        detail::writeLE(out, static_cast<uint16_t>(a->targetId), 2);
        detail::writeLE(out, ctx.timeMs, 4);
    }
    else if (const auto* c = std::get_if<Common>(&cmd))
    {
        detail::writeLE(out, kMsgIdCommon, 4);
        detail::writeLE(out, static_cast<uint16_t>(c->cmd), 2);
        detail::writeLE(out, static_cast<uint16_t>(c->x), 2);
        detail::writeLE(out, static_cast<uint16_t>(c->y), 2);
        out.push_back(c->dir);
        // v1 = the inventory slot; "no slot" is absent on the wire, and the
        // reference client's zero-initialized struct puts 0 there.
        const int32_t v1 = (c->slot < 0) ? 0 : c->slot;
        detail::writeLE(out, static_cast<uint32_t>(v1), 4);
        detail::writeLE(out, static_cast<uint32_t>(c->idx), 4); // v2
        detail::writeLE(out, 0u, 4);                            // v3: unused by these commands
        detail::writeLE(out, ctx.timeMs, 4);
    }

    return out;
}

} // namespace Protocol