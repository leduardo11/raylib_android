#pragma once

namespace Core {

class GameLoop {
public:
    using UpdateFn = void(*)(float dt, void* user);
    using RenderFn = void(*)(void* user);

    GameLoop();
    void run(UpdateFn onUpdate, RenderFn onRender, void* user);

    float deltaTime() const { return m_dt; }
    float fps() const       { return m_fps; }

private:
    float m_dt = 0.0f;
    float m_fps = 0.0f;
    int   m_frameCount = 0;
    float m_fpsTimer = 0.0f;
};

} // namespace Core
