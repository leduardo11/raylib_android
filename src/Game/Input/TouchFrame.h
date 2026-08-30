#pragma once

#include <cstdint>

// TouchFrame: engine-agnostic multitouch snapshot consumed by the HUD layer.
// Systems::Input fills it from raylib (and synthesizes a single mouse "touch"
// on desktop so the HUD behaves identically in dev). The HUD never touches
// raylib's touch API directly.

namespace Input {

struct TouchPoint {
    int   id = -1;           // finger id, stable for the whole press
    float x = 0.0f;          // logical screen coords
    float y = 0.0f;
    bool  justPressed  = false; // rising edge this frame
    bool  down         = false; // held this frame
    bool  justReleased = false; // falling edge this frame
};

inline constexpr uint16_t kMaxTrackedTouches = 10;

struct TouchFrame {
    uint16_t count = 0;
    TouchPoint points[kMaxTrackedTouches]{};

    const TouchPoint* find(int id) const
    {
        for (uint16_t i = 0; i < count; ++i)
            if (points[i].id == id) return &points[i];
        return nullptr;
    }
};

} // namespace Input