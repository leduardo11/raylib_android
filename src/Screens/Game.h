#pragma once

#include "Core/Screen.h"
#include "Game/Hud/MobileControlsHud.h"
#include "Game/Input/PlayerInputFrame.h"
#include "Game/Protocol/HelbreathPacketEncoder.h"
#include "Game/Simulation/GridPlayWorld.h"
#include "Game/Simulation/GridWorld.h"
#include "Game/Simulation/NavExecutor.h"
#include "Game/Simulation/PlayerMovementSimulation.h"
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
    Simulation::GridPlayWorld m_gameWorld;
    Simulation::NavExecutor   m_nav;
    HUD::MobileControlsHud    m_hud;
    Presentation::Camera      m_camera;
    Presentation::MapRenderer m_map;
    Presentation::PlayerSprite m_sprite;

    Input::PlayerInputFrame m_frame;
    Protocol::EncodeContext m_encodeCtx;
    uint64_t m_timeMs = 0;

    // Manual-joystick interruption of navigation: while a Move flows from the
    // joystick/keys the nav is suspended (kept for resume on release).
    bool m_manualActive = false;
    bool m_prevManualActive = false;

    bool m_mapLoaded = false;

    void initWorld();
    void routeFrame();
    void emitProtocol(const Input::PlayerCommand& cmd);
    Simulation::GridCoord screenToTile(float sx, float sy) const;
    void drawGrid();
    void drawPlayer();
    void drawHud();
    void drawTargetMarkers();
};

} // namespace Screens