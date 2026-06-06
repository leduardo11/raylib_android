#include "Splash.h"
#include "MainMenu.h"
#include "Core/Application.h"
#include "Systems/Input.h"
#include "Systems/Rendering.h"

namespace Screens {

Splash::Splash(Core::Application& app)
    : m_app(app)
{
}

void Splash::update(float dt)
{
    m_timer += dt;

    // Skip to main menu on tap/keypress after 1s
    if ((m_timer > 1.0f && m_app.input().isPointerPressed()) || m_timer > 3.0f)
        m_app.setScreen(new MainMenu(m_app));
}

void Splash::render()
{
    Systems::Rendering::clear({ 0x11, 0x11, 0x22, 0xFF });
    Systems::Rendering::textCentered("raylib Android", m_app.width() / 2.0f,
                                      m_app.height() / 2.0f - 30, 36,
                                      { 0xFF, 0xAA, 0x44, 0xFF });
    Systems::Rendering::textCentered("Tap to continue", m_app.width() / 2.0f,
                                      m_app.height() / 2.0f + 30, 18,
                                      { 0x88, 0x88, 0x88, 0xFF });
}

} // namespace Screens
