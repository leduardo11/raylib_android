#include "Application.h"
#include "GameLoop.h"
#include "Screen.h"
#include "Platform.h"
#include "Systems/Input.h"
#include "Systems/Audio.h"
#include "raylib.h"

namespace Core {

Application::Application(const char* title, int width, int height)
    : m_title(title)
    , m_width(width)
    , m_height(height)
{
    Platform::init();
    InitWindow(m_width, m_height, m_title);

    m_loop  = new GameLoop();
    m_input = new Systems::Input();
    m_audio = new Systems::Audio();
}

Application::~Application()
{
    delete m_audio;
    delete m_input;
    delete m_loop;
    delete m_screen;

    if (WindowShouldClose() == false)
        CloseWindow();
}

void Application::run()
{
    m_loop->run(onUpdate, onRender, this);
    CloseWindow();
}

void Application::quit()
{
    m_running = false;
}

void Application::setScreen(Screen* screen)
{
    m_next = screen;
}

void Application::onUpdate(float dt, void* user)
{
    auto* app = static_cast<Application*>(user);

    app->m_input->update();

    if (app->m_next)
    {
        if (app->m_screen)
            app->m_screen->onExit();
        delete app->m_screen;
        app->m_screen = app->m_next;
        app->m_next = nullptr;
        app->m_screen->onEnter();
    }

    if (app->m_screen && app->m_running)
        app->m_screen->update(dt);
}

void Application::onRender(void* user)
{
    auto* app = static_cast<Application*>(user);
    if (app->m_screen && app->m_running)
        app->m_screen->render();
}

} // namespace Core
