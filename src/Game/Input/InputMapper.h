#pragma once

#include "Game/Simulation/MoveIntent.h"
#include "JoystickInput.h"
#include "KeyboardInput.h"

namespace Input {

// Produces a single source-agnostic MoveIntent from whatever device is active.
// Touch/mouse joystick takes priority; keyboard falls back when it is pressed.
class InputMapper {
public:
    void update(JoystickInput& joystick);

    const Simulation::MoveIntent& intent() const { return m_intent; }

private:
    Simulation::MoveIntent m_intent;
};

} // namespace Input