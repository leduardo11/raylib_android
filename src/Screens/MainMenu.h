#pragma once

#include "Core/Screen.h"
#include "Systems/UI.h"

namespace Core { class Application; }

namespace Screens {

class MainMenu : public Core::Screen {
public:
    explicit MainMenu(class Core::Application& app);
    void onEnter() override;
    void update(float dt) override;
    void render() override;

private:
    Core::Application& m_app;
    Systems::Button m_btnPlay;
    Systems::Button m_btnSettings;
    Systems::Button m_btnExit;
};

} // namespace Screens
