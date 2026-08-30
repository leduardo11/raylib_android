#pragma once

#include "PlayerCommand.h"

#include <cstdint>

// PlayerInputFrame: one tick's worth of player intent, produced by an input
// producer (joystick, keyboard/mouse, touch HUD, network) and consumed by the
// sim/nav and protocol translators.
//
// A fixed-capacity command array keeps producers allocation-free and makes
// the frame a plain value type — it stays copyable/testable with zero I/O.

namespace Input {

constexpr uint16_t kPlayerCommandsPerFrame = 16;

struct PlayerInputFrame {
    uint64_t frameIndex = 0;

private:
    PlayerCommand m_commands[kPlayerCommandsPerFrame]{};
    uint16_t m_count = 0;

public:
    uint16_t count() const { return m_count; }
    const PlayerCommand* begin() const { return m_commands; }
    const PlayerCommand* end() const { return m_commands + m_count; }

    // Appends a command; returns false when the frame is full (drop, don't grow).
    bool push(PlayerCommand cmd)
    {
        if (m_count >= kPlayerCommandsPerFrame) return false;
        m_commands[m_count++] = cmd;
        return true;
    }

    void clear() { m_count = 0; }
};

} // namespace Input