#pragma once

#include "Core/Screen.h"

namespace Core { class Application; }

namespace Screens {

class Splash : public Core::Screen {
public:
    explicit Splash(class Core::Application& app);
    void update(float dt) override;
    void render() override;

private:
    Core::Application& m_app;
    float m_timer = 0.0f;
};

} // namespace Screens
