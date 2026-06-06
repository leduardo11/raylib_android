#include "MainMenu.h"
#include "Game.h"
#include "Core/Application.h"
#include "Systems/Input.h"
#include "Systems/Rendering.h"
#include "raylib.h"

namespace Screens {

MainMenu::MainMenu(Core::Application& app)
    : m_app(app)
    , m_btnPlay(Systems::UI::makeButton("Play",     app.width() / 2.0f, app.height() / 2.0f - 64 * 1.5f, 280, 64))
    , m_btnSettings(Systems::UI::makeButton("Settings", app.width() / 2.0f, app.height() / 2.0f - 64 * 0.5f, 280, 64))
    , m_btnExit(Systems::UI::makeButton("Exit",     app.width() / 2.0f, app.height() / 2.0f + 64 * 0.5f, 280, 64))
{
}

void MainMenu::onEnter()
{
    TraceLog(LOG_INFO, "Entered MainMenu");
}

void MainMenu::update(float dt)
{
    (void)dt;
    Vector2 pointer = m_app.input().touchPos();
    bool pressed = m_app.input().isPointerPressed();

    Systems::UI::updateButton(m_btnPlay,     pointer, pressed);
    Systems::UI::updateButton(m_btnSettings, pointer, pressed);
    Systems::UI::updateButton(m_btnExit,     pointer, pressed);

    if (m_btnPlay.clicked)
        m_app.setScreen(new Game(m_app));
    else if (m_btnSettings.clicked)
        TraceLog(LOG_INFO, "Settings clicked");
    else if (m_btnExit.clicked)
        m_app.quit();
}

void MainMenu::render()
{
    Systems::Rendering::clear({ 0x11, 0x11, 0x22, 0xFF });
    Systems::Rendering::textCentered("raylib on Android", m_app.width() / 2.0f, 60, 36,
                                      { 0xFF, 0xAA, 0x44, 0xFF });

    auto cNorm = Color{ 0x22, 0x22, 0x44, 0xFF };
    auto cHov  = Color{ 0x44, 0x88, 0xFF, 0xFF };
    auto cOut  = Color{ 0x88, 0xAA, 0xFF, 0xFF };

    Systems::UI::drawButton(m_btnPlay,     cNorm, cHov, cOut);
    Systems::UI::drawButton(m_btnSettings, cNorm, cHov, cOut);
    Systems::UI::drawButton(m_btnExit,     cNorm, cHov, cOut);
}

} // namespace Screens
