#pragma once

#include "Game/Input/KeyState.h"
#include "Game/Input/TouchFrame.h"
#include "raylib.h"
#include <cstdint>

namespace Systems {

struct Input {
    void update();

    const ::Input::TouchFrame& touchFrame() const { return m_touchFrame; }
    const ::Input::KeyState& keyState() const { return m_keyState; }

    bool isPressed(int key)  const { return IsKeyPressed(key); }
    bool isDown(int key)     const { return IsKeyDown(key); }
    bool isTouched()         const { return GetTouchPointCount() > 0; }

    bool isPointerDown() const
    {
        return IsMouseButtonDown(MOUSE_BUTTON_LEFT) || GetTouchPointCount() > 0;
    }

    Vector2 touchPos() const
    {
        if (GetTouchPointCount() > 0)
            return GetTouchPosition(0);
        return GetMousePosition();
    }

    bool isPointerPressed() const
    {
        return IsMouseButtonPressed(MOUSE_BUTTON_LEFT) || IsGestureDetected(GESTURE_TAP);
    }

    float getWheelMove() const { return GetMouseWheelMove(); }

private:
    ::Input::TouchFrame m_touchFrame;
    ::Input::KeyState   m_keyState;
};

} // namespace Systems