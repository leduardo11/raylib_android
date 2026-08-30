#include "Game.h"
#include "MainMenu.h"
#include "Core/Application.h"
#include "Systems/Input.h"
#include "Systems/Rendering.h"
#include "Game/Presentation/PlayerPresentationState.h"
#include "raylib.h"

namespace Screens {

namespace {

constexpr float TILE_SIZE  = 32.0f;
constexpr float LOGICAL_W  = 1280.0f;
constexpr float LOGICAL_H  = 720.0f;
constexpr float VIEW_W     = LOGICAL_W;
constexpr float VIEW_H     = LOGICAL_H;

// Fallback world used only when the cs_rpg map package is missing.
constexpr int   FALLBACK_MAP_W = 30;
constexpr int   FALLBACK_MAP_H = 20;
constexpr float FALLBACK_VIEW_W = 800.0f;
constexpr float FALLBACK_VIEW_H = 600.0f;

constexpr float JOYSTICK_ZONE_W = LOGICAL_W * 0.70f; // left band grabs joystick

constexpr Color CLR_BG        = { 0x12, 0x14, 0x1C, 0xFF };
constexpr Color CLR_WALKABLE  = { 0x22, 0x2A, 0x38, 0xFF };
constexpr Color CLR_WALK_HIL  = { 0x2A, 0x35, 0x46, 0xFF };
constexpr Color CLR_BLOCKED   = { 0x16, 0x16, 0x20, 0xFF };
constexpr Color CLR_GRID      = { 0x0B, 0x0E, 0x14, 0xFF };
constexpr Color CLR_SPAWN     = { 0x3F, 0x6E, 0x8F, 0xFF };

} // namespace

Game::Game(Core::Application& app)
    : m_app(app)
    , m_btnBack(Systems::UI::makeButton("Back", 1280 - 116, 22, 100, 36))
{
}

void Game::onEnter()
{
    initWorld();
    TraceLog(LOG_INFO, "Entered Game screen (GridPlay)");
}

void Game::onExit()
{
    TraceLog(LOG_INFO, "Exited Game screen (GridPlay)");
}

void Game::initWorld()
{
    m_mapLoaded = m_map.load("maps");
    m_sprite.load("entities/player/player.pkg.json");

    if (m_mapLoaded)
    {
        const Content::TileMapData& d = m_map.data();

        m_world.setSize(d.width, d.height);
        for (int y = 0; y < d.height; ++y)
            for (int x = 0; x < d.width; ++x)
                m_world.setWalkable(x, y,
                                    d.walkable[(size_t)y * d.width + x]);

        m_sim.setWorld(&m_world);
        m_sim.setTilePosition(d.spawnTileX, d.spawnTileY);

        m_camera.init((float)(d.width * d.tileSize),
                      (float)(d.height * d.tileSize),
                      VIEW_W, VIEW_H);
        m_camera.reset(Vector2{ (float)(d.spawnTileX * d.tileSize +
                                        d.tileSize / 2),
                                (float)(d.spawnTileY * d.tileSize +
                                        d.tileSize / 2) });
        TraceLog(LOG_INFO, "Game: map %dx%d, spawn %d,%d", d.width, d.height,
                d.spawnTileX, d.spawnTileY);
    }
    else
    {
        TraceLog(LOG_WARNING, "Game: map package missing — using fallback grid");
        m_world.setSize(FALLBACK_MAP_W, FALLBACK_MAP_H);
        m_world.markSimpleMap();

        m_sim.setWorld(&m_world);
        m_sim.setTilePosition(15, 10);

        m_camera.init(FALLBACK_MAP_W * TILE_SIZE,
                      FALLBACK_MAP_H * TILE_SIZE,
                      FALLBACK_VIEW_W, FALLBACK_VIEW_H);
        m_camera.reset(Vector2{ 15 * TILE_SIZE, 10 * TILE_SIZE });
    }
}

void Game::handleInput()
{
    Vector2 pointer = m_app.input().touchPos();
    bool pointerDown = m_app.input().isPointerDown();
    bool pointerPressed = m_app.input().isPointerPressed();

    Systems::UI::updateButton(m_btnBack, pointer, pointerPressed);
    if (m_btnBack.clicked)
    {
        m_app.setScreen(new MainMenu(m_app));
        return;
    }

    if (pointerDown && !m_joystick.active() &&
        pointer.x < JOYSTICK_ZONE_W)
    {
        m_joystick.update(pointer, true);
    }
    else if (m_joystick.active() && pointerDown)
    {
        m_joystick.update(pointer, true);
    }
    else
    {
        m_joystick.update(pointer, false);
    }

    m_mapper.update(m_joystick);

    const auto& intent = m_mapper.intent();
    if (intent.active)
    {
        Simulation::Locomotion loco = intent.locomotion;
        if (m_walkMode)
            loco = Simulation::Locomotion::Walking;
        m_sim.handleInput(intent.direction, loco);
    }
    else
    {
        m_sim.releaseInput();
    }
}

void Game::update(float dt)
{
    if (IsKeyPressed(KEY_TAB) || IsKeyPressed(KEY_LEFT_SHIFT) ||
        IsKeyPressed(KEY_RIGHT_SHIFT))
        m_walkMode = !m_walkMode;

    handleInput();

    float dtMs = dt * 1000.0f;
    m_sim.update(dtMs);

    auto pres = m_sim.presentation(TILE_SIZE);

    Presentation::PlayerPresentationState state;
    state.position      = Vector2{ pres.pixelX, pres.pixelY };
    state.facing        = pres.facing;
    state.locomotion    = pres.locomotion;
    state.stepProgress  = pres.stepProgress;
    state.isMoving      = pres.isMoving;

    m_sprite.update(state, dtMs);
    m_camera.update(dt, Vector2{ pres.pixelX, pres.pixelY });
}

void Game::render()
{
    Systems::Rendering::clear(CLR_BG);

    m_camera.apply();
    if (m_mapLoaded)
        m_map.draw(m_camera);
    else
        drawGrid();
    drawPlayer();
    m_camera.restore();

    drawHud();
    drawJoystick();
    Systems::UI::drawButton(m_btnBack,
                            { 0x33, 0x33, 0x55, 0xFF },
                            { 0x55, 0x55, 0x88, 0xFF },
                            { 0x88, 0xAA, 0xFF, 0xFF });
}

void Game::drawGrid()
{
    int camX = (int)(m_camera.origin().x / TILE_SIZE) - 1;
    int camY = (int)(m_camera.origin().y / TILE_SIZE) - 1;
    int viewTilesX = (int)(FALLBACK_VIEW_W / TILE_SIZE) + 3;
    int viewTilesY = (int)(FALLBACK_VIEW_H / TILE_SIZE) + 3;

    for (int y = camY; y < camY + viewTilesY; ++y)
    {
        for (int x = camX; x < camX + viewTilesX; ++x)
        {
            if (!m_world.isInBounds(x, y))
                continue;

            Vector2 pos{ x * TILE_SIZE, y * TILE_SIZE };
            Rectangle tile{ pos.x, pos.y, TILE_SIZE, TILE_SIZE };

            if (m_world.isWalkable(x, y))
            {
                DrawRectangleRec(tile, CLR_WALKABLE);
                if ((x + y) % 2 == 0)
                    DrawRectangleRec(tile, CLR_WALK_HIL);
            }
            else
            {
                DrawRectangleRec(tile, CLR_BLOCKED);
            }
            DrawRectangleLinesEx(tile, 1.0f, CLR_GRID);
        }
    }
}

void Game::drawPlayer()
{
    auto pres = m_sim.presentation(TILE_SIZE);

    Presentation::PlayerPresentationState state;
    state.position      = Vector2{ pres.pixelX, pres.pixelY };
    state.facing        = pres.facing;
    state.locomotion    = pres.locomotion;
    state.stepProgress  = pres.stepProgress;
    state.isMoving      = pres.isMoving;

    m_sprite.draw(state, TILE_SIZE);
}

void Game::drawHud()
{
    SetTextLineSpacing(16);
    Systems::Rendering::text("Helbreath Map", 16, 14, 22,
                             { 0xFC, 0xE9, 0xB0, 0xFF });
    Systems::Rendering::text("WASD / arrows / drag joystick", 16, 44, 14,
                             { 0x88, 0x99, 0xAA, 0xFF });

    auto pres = m_sim.presentation(TILE_SIZE);
    const char* mode = m_walkMode ? "WALK 560ms" : "RUN 312ms";
    const char* loco = (pres.locomotion == Simulation::Locomotion::Running)
                           ? "Running" : "Walking";
    if (!pres.isMoving) loco = "Standing";

    Systems::Rendering::text(
        TextFormat("tile  %d,%d    dir  %d    %s", (int)m_sim.tilePosition().x,
                   (int)m_sim.tilePosition().y, (int)pres.facing, loco),
        16, 64, 14, { 0x88, 0x99, 0xAA, 0xFF });
    Systems::Rendering::text(
        TextFormat("mode  %s    (Shift/Tab toggles)", mode),
        16, 84, 14, { 0x88, 0x99, 0xAA, 0xFF });
}

void Game::drawJoystick()
{
    if (!m_joystick.active())
        return;

    float r = 54.0f;
    DrawCircleV(m_joystick.origin(), r,
                Fade(CLR_WALK_HIL, 0.55f));
    DrawCircleLinesV(m_joystick.origin(), r,
                     Fade({ 0x8A, 0xA0, 0xC0, 0xFF }, 0.6f));
    DrawCircleV(m_joystick.current(), 18.0f,
                Fade({ 0x6A, 0xE0, 0x8A, 0xFF }, 0.7f));
}

} // namespace Screens