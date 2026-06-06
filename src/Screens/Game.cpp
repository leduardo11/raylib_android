#include "Game.h"
#include "MainMenu.h"
#include "Core/Application.h"
#include "Systems/Input.h"
#include "Systems/Rendering.h"
#include "raylib.h"

namespace Screens {

Game::Game(Core::Application& app)
    : m_app(app)
    , m_btnBack(Systems::UI::makeButton("< Back", 80, app.height() - 50, 140, 36))
{
}

void Game::onEnter()
{
    TraceLog(LOG_INFO, "Entered Game screen");
}

void Game::onExit()
{
    TraceLog(LOG_INFO, "Exited Game screen");
}

void Game::update(float dt)
{
    m_rotation += dt * 90.0f;
    if (m_rotation > 360.0f) m_rotation -= 360.0f;

    Vector2 pointer = m_app.input().touchPos();
    bool pressed = m_app.input().isPointerPressed();
    Systems::UI::updateButton(m_btnBack, pointer, pressed);

    if (m_btnBack.clicked)
        m_app.setScreen(new MainMenu(m_app));
}

void Game::render()
{
    Systems::Rendering::clear({ 0x22, 0x11, 0x33, 0xFF });

    // Demo: spinning rectangle
    float cx = m_app.width() / 2.0f;
    float cy = m_app.height() / 2.0f;
    DrawRectanglePro({ cx, cy, 80, 80 }, { 40, 40 }, m_rotation,
                     { 0x44, 0xFF, 0x88, 0xFF });

    Systems::Rendering::textCentered("Game Screen", cx, 60, 28,
                                      { 0xFF, 0xAA, 0x44, 0xFF });

    Systems::UI::drawButton(m_btnBack,
                            { 0x33, 0x33, 0x55, 0xFF },
                            { 0x55, 0x55, 0x88, 0xFF },
                            { 0x88, 0xAA, 0xFF, 0xFF });
}

} // namespace Screens
