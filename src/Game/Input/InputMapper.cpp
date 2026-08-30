#include "InputMapper.h"

namespace Input {

void InputMapper::update(JoystickInput& joystick)
{
    m_intent.active = false;
    m_intent.locomotion = Simulation::Locomotion::Standing;

    if (joystick.active() &&
        joystick.magnitude() >= JOYSTICK_DEAD_ZONE)
    {
        m_intent.direction  = joystick.direction();
        m_intent.active     = true;
        m_intent.locomotion = Simulation::Locomotion::Running;
        return;
    }

    float kx, ky;
    if (readKeyboardVector(kx, ky))
        m_intent = keyboardMoveIntent(kx, ky);
}

} // namespace Input