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

void Camera::setZoom(float zoom)
{
    if (zoom > 0.0f)
        m_zoom = zoom;
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
    const float viewW = m_viewW / m_zoom;
    const float viewH = m_viewH / m_zoom;

    Vector2 min{ 0.0f, 0.0f };
    Vector2 max{ m_worldW - viewW, m_worldH - viewH };

    if (max.x < min.x) { max.x = (m_worldW - viewW) * 0.5f; min.x = max.x; }
    if (max.y < min.y) { max.y = (m_worldH - viewH) * 0.5f; min.y = max.y; }

    m_origin.x = std::clamp(m_focus.x - viewW * 0.5f, min.x, max.x);
    m_origin.y = std::clamp(m_focus.y - viewH * 0.5f, min.y, max.y);
}

void Camera::apply()
{
    BeginMode2D((Camera2D){
        Vector2{ 0, 0 },
        m_origin,
        0.0f,
        m_zoom
    });
}

void Camera::restore()
{
    EndMode2D();
}

} // namespace Presentation