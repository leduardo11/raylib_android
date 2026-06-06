#include "GameLoop.h"
#include "raylib.h"

namespace Core {

GameLoop::GameLoop()
{
    SetTargetFPS(60);
}

void GameLoop::run(UpdateFn onUpdate, RenderFn onRender, void* user)
{
    while (!WindowShouldClose())
    {
        m_dt = GetFrameTime();

        // FPS counter
        m_frameCount++;
        m_fpsTimer += m_dt;
        if (m_fpsTimer >= 1.0f)
        {
            m_fps = m_frameCount / m_fpsTimer;
            m_frameCount = 0;
            m_fpsTimer = 0.0f;
        }

        if (onUpdate)
            onUpdate(m_dt, user);

        BeginDrawing();
        if (onRender)
            onRender(user);
        EndDrawing();
    }
}

} // namespace Core
