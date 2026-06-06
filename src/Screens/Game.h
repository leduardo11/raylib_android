#pragma once

#include "Core/Screen.h"
#include "Systems/UI.h"

namespace Core { class Application; }

namespace Screens {

class Game : public Core::Screen {
public:
    explicit Game(class Core::Application& app);
    void onEnter() override;
    void onExit()  override;
    void update(float dt) override;
    void render() override;

private:
    Core::Application& m_app;
    Systems::Button m_btnBack;
    float m_rotation = 0.0f;
};

} // namespace Screens
