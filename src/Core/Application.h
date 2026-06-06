#pragma once

namespace Systems { class Input; class Audio; }

namespace Core {

class GameLoop;
struct Screen;

class Application {
public:
    Application(const char* title, int width, int height);
    ~Application();

    void run();
    void quit();

    void setScreen(Screen* screen);
    Screen* currentScreen() const { return m_screen; }

    Systems::Input& input() const { return *m_input; }
    Systems::Audio& audio() const { return *m_audio; }

    int width()  const { return m_width; }
    int height() const { return m_height; }

private:
    static void onUpdate(float dt, void* user);
    static void onRender(void* user);

    GameLoop*      m_loop    = nullptr;
    Systems::Input* m_input  = nullptr;
    Systems::Audio* m_audio  = nullptr;
    Screen*        m_screen  = nullptr;
    Screen*        m_next    = nullptr;
    const char*    m_title;
    int m_width, m_height;
    bool m_running = true;
};

} // namespace Core
