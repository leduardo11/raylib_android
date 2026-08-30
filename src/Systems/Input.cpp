#include "Systems/Input.h"

namespace Systems {

void Input::update()
{
    // ── Multitouch frame ─────────────────────────────────────────────────
    // Build the current down-set first (real pointers; a single synthetic
    // mouse "touch" on desktop for dev parity), then diff against the previous
    // frame to derive press/release edges. Released points appear in the frame
    // with down=false so producers can observe the falling edge.
    ::Input::TouchFrame cur;

    const int tc = GetTouchPointCount();
    if (tc > 0)
    {
        const int n = (tc < ::Input::kMaxTrackedTouches)
                          ? tc : (int)::Input::kMaxTrackedTouches;
        for (int i = 0; i < n; ++i)
        {
            const Vector2 pos = GetTouchPosition(i);
            cur.points[cur.count++] = ::Input::TouchPoint{
                GetTouchPointId(i), pos.x, pos.y, false, true, false };
        }
    }
    else if (IsMouseButtonDown(MOUSE_BUTTON_LEFT))
    {
        const Vector2 pos = GetMousePosition();
        cur.points[cur.count++] = ::Input::TouchPoint{ -1, pos.x, pos.y,
                                                     false, true, false };
    }

    // Rising edges: was not down last frame.
    for (uint16_t i = 0; i < cur.count; ++i)
    {
        const ::Input::TouchPoint* prev = m_touchFrame.find(cur.points[i].id);
        if (prev && prev->down)
            continue;                    // was already down
        cur.points[i].justPressed = true;
    }

    // Falling edges: was down last frame, not down this frame.
    for (uint16_t i = 0; i < m_touchFrame.count; ++i)
    {
        const ::Input::TouchPoint& prev = m_touchFrame.points[i];
        if (!prev.down) continue;
        if (cur.find(prev.id)) continue; // still down this frame
        if (cur.count >= ::Input::kMaxTrackedTouches) break;
        cur.points[cur.count++] = ::Input::TouchPoint{
            prev.id, prev.x, prev.y, false, false, true };
    }

    m_touchFrame = cur;

    // ── Device keys (desktop; all false on Android) ──────────────────────
    ::Input::KeyState k{};
    k.moveUp    = IsKeyDown(KEY_W)    || IsKeyDown(KEY_UP);
    k.moveDown  = IsKeyDown(KEY_S)    || IsKeyDown(KEY_DOWN);
    k.moveLeft  = IsKeyDown(KEY_A)    || IsKeyDown(KEY_LEFT);
    k.moveRight = IsKeyDown(KEY_D)    || IsKeyDown(KEY_RIGHT);
    k.shiftHeld = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    k.ctrlHeld  = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);

    k.tabStance   = IsKeyPressed(KEY_TAB);
    k.homeSafe    = IsKeyPressed(KEY_HOME);
    k.ctrlAForce  = k.ctrlHeld && IsKeyPressed(KEY_A);
    k.ctrlRRun    = k.ctrlHeld && IsKeyPressed(KEY_R);
    k.pageUp      = IsKeyPressed(KEY_PAGE_UP);
    k.f2 = IsKeyPressed(KEY_F2);
    k.f3 = IsKeyPressed(KEY_F3);
    k.f4 = IsKeyPressed(KEY_F4);
    k.f5 = IsKeyPressed(KEY_F5);
    k.f6 = IsKeyPressed(KEY_F6);
    k.f7 = IsKeyPressed(KEY_F7);
    k.f8 = IsKeyPressed(KEY_F8);
    k.f9 = IsKeyPressed(KEY_F9);
    k.f10 = IsKeyPressed(KEY_F10);
    k.hpInsertHeld = IsKeyDown(KEY_INSERT);
    k.mpDeleteHeld = IsKeyDown(KEY_DELETE);

    m_keyState = k;
}

} // namespace Systems