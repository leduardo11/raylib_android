#include "Camera.h"

#include <cmath>
#include <algorithm>

namespace Presentation {

void Camera::init(float worldWidthPx, float worldHeightPx,
                  float viewportWidth, float viewportHeight)
{
    m_worldW = worldWidthPx;
    m_worldH = worldHeightPx;
    m_viewW  = viewportWidth;
    m_viewH  = viewportHeight;
}

void Camera::reset(Vector2 focus)
{
    m_focus = focus;
    clampOrigin();
}

void Camera::update(float dt, Vector2 focus)
{
    float k = CAM_LERP;
    m_focus.x += (focus.x - m_focus.x) * (1.0f - expf(-k * dt));
    m_focus.y += (focus.y - m_focus.y) * (1.0f - expf(-k * dt));
    clampOrigin();
}

void Camera::clampOrigin()
{
    Vector2 min{ 0.0f, 0.0f };
    Vector2 max{ m_worldW - m_viewW, m_worldH - m_viewH };

    if (max.x < min.x) { max.x = (m_worldW - m_viewW) * 0.5f; min.x = max.x; }
    if (max.y < min.y) { max.y = (m_worldH - m_viewH) * 0.5f; min.y = max.y; }

    m_origin.x = std::clamp(m_focus.x - m_viewW * 0.5f, min.x, max.x);
    m_origin.y = std::clamp(m_focus.y - m_viewH * 0.5f, min.y, max.y);
}

void Camera::apply()
{
    BeginMode2D((Camera2D){
        m_origin,
        Vector2{ 0, 0 },
        0.0f,
        1.0f
    });
}

void Camera::restore()
{
    EndMode2D();
}

} // namespace Presentation