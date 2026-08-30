#pragma once

#include "Game/Simulation/HelbreathDirection.h"
#include "Game/Simulation/MoveIntent.h"
#include "Game/Simulation/MovementTiming.h"
#include "DirectionQuantizer.h"
#include "raylib.h"
#include <cmath>

namespace Input {

constexpr float JOYSTICK_DEAD_ZONE = 0.22f;
constexpr float JOYSTICK_SECTOR_COUNT = 8.0f;

// Symmetric 8-way quantization: dead zone -> atan2 -> equal 45-degree sectors.
inline Simulation::Direction quantizeJoystickVector(float dx, float dy,
                                                    float deadZone = JOYSTICK_DEAD_ZONE)
{
    float magnitude = sqrtf(dx * dx + dy * dy);
    if (magnitude < deadZone)
        return Simulation::Direction::S;

    return DirectionQuantizer::fromVector(dx / magnitude, dy / magnitude);
}

// Floating virtual joystick. Activates on touch-down, center is established at
// the initial touch, drag produces a vector relative to that center. Desktop
// mouse-drag emulates the joystick for development.
class JoystickInput {
public:
    void update(Vector2 pointer, bool pointerDown);

    bool active() const { return m_active; }
    float vectorX() const { return m_vectorX; }
    float vectorY() const { return m_vectorY; }
    Vector2 origin() const { return m_origin; }
    Vector2 current() const { return m_current; }

    Simulation::Direction direction() const;

private:
    bool    m_active   = false;
    Vector2 m_origin   = { 0, 0 };
    Vector2 m_current  = { 0, 0 };
    float   m_vectorX  = 0.0f;
    float   m_vectorY  = 0.0f;
    float   m_radius   = 0.0f;

    void compute();
};

} // namespace Input