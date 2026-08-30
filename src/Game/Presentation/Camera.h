#pragma once

#include "raylib.h"

namespace Presentation {

constexpr float CAM_LERP = 4.0f;

class Camera {
public:
    Vector2 origin() const { return m_origin; }
    float viewWidth()  const { return m_viewW; }
    float viewHeight() const { return m_viewH; }

    void init(float worldWidthPx, float worldHeightPx,
              float viewportWidth, float viewportHeight);
    void reset(Vector2 focus);
    void update(float dt, Vector2 focus);
    void apply();
    void restore();

private:
    Vector2 m_origin = { 0, 0 };
    Vector2 m_focus  = { 0, 0 };
    float m_worldW = 0.0f;
    float m_worldH = 0.0f;
    float m_viewW  = 0.0f;
    float m_viewH  = 0.0f;

    void clampOrigin();
};

} // namespace Presentation