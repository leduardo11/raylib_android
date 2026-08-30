#pragma once

#include "Core/Screen.h"
#include "Game/Simulation/GridWorld.h"
#include "Game/Simulation/PlayerMovementSimulation.h"
#include "Game/Input/JoystickInput.h"
#include "Game/Input/InputMapper.h"
#include "Game/Presentation/Camera.h"
#include "Game/Presentation/MapRenderer.h"
#include "Game/Presentation/PlayerSprite.h"
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

    Simulation::GridWorld    m_world;
    Simulation::PlayerMovementSimulation m_sim;
    Input::JoystickInput     m_joystick;
    Input::InputMapper       m_mapper;
    Presentation::Camera     m_camera;
    Presentation::MapRenderer m_map;
    Presentation::PlayerSprite m_sprite;

    bool m_walkMode = true;
    bool m_mapLoaded = false;

    void initWorld();
    void handleInput();
    void drawGrid();
    void drawPlayer();
    void drawHud();
    void drawJoystick();
};

} // namespace Screens