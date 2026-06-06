#pragma once

#include "raylib.h"

namespace Systems {

struct Input {
    void update();

    bool isPressed(int key)  const { return IsKeyPressed(key); }
    bool isDown(int key)     const { return IsKeyDown(key); }
    bool isTouched()         const { return GetTouchPointCount() > 0; }

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
};

} // namespace Systems
