#include "JoystickInput.h"

#include <cmath>

namespace Input {

void JoystickInput::update(Vector2 pointer, bool pointerDown)
{
    if (pointerDown && !m_active)
    {
        m_active = true;
        m_origin = pointer;
        m_current = pointer;
        m_radius = 0.0f;
        m_vectorX = 0.0f;
        m_vectorY = 0.0f;
        return;
    }

    if (!pointerDown)
    {
        m_active = false;
        m_vectorX = 0.0f;
        m_vectorY = 0.0f;
        return;
    }

    m_current = pointer;
    compute();
}

void JoystickInput::compute()
{
    float dx = m_current.x - m_origin.x;
    float dy = m_current.y - m_origin.y;
    m_radius = sqrtf(dx * dx + dy * dy);

    if (m_radius > 0.0f)
    {
        m_vectorX = dx / m_radius;
        m_vectorY = dy / m_radius;
    }
    else
    {
        m_vectorX = 0.0f;
        m_vectorY = 0.0f;
    }
}

Simulation::Direction JoystickInput::direction() const
{
    return quantizeJoystickVector(m_vectorX, m_vectorY);
}

} // namespace Input