#include "Game.h"
#include "MainMenu.h"
#include "Core/Application.h"
#include "Systems/Input.h"
#include "Systems/Rendering.h"
#include "Game/Presentation/PlayerPresentationState.h"
#include "Game/Protocol/CommandTranslator.h"
#include "Game/Protocol/ProtocolCommand.h"
#include "raylib.h"
#include "rlgl.h"

#include <cstdlib>

namespace Screens {

#if defined(__ANDROID__)
    #define GRIDPLAY_GETENV(name) ((const char*)nullptr)
#else
    #include <cstdlib>
    #define GRIDPLAY_GETENV(name) std::getenv(name)
#endif

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

// Camera zoom: zoom 1.0 shows 40x22.5 tiles (LOGICAL_W / TILE_SIZE) — far too
// wide for mobile. Standard ARPG feel keeps the player large and the visible
// map tight. 2.0 -> 20x11.25 tiles; tile the map texture is upscaled.
constexpr float CAM_ZOOM = 2.0f;

constexpr Color CLR_BG        = { 0x12, 0x14, 0x1C, 0xFF };
constexpr Color CLR_WALKABLE  = { 0x22, 0x2A, 0x38, 0xFF };
constexpr Color CLR_WALK_HIL  = { 0x2A, 0x35, 0x46, 0xFF };
constexpr Color CLR_BLOCKED   = { 0x16, 0x16, 0x20, 0xFF };
constexpr Color CLR_GRID      = { 0x0B, 0x0E, 0x14, 0xFF };
constexpr Color CLR_SPAWN     = { 0x3F, 0x6E, 0x8F, 0xFF };

constexpr Color CLR_RETICLE   = { 0x9E, 0xE6, 0x4F, 0xFF }; // valid (green)
constexpr Color CLR_RETICLE_B = { 0xE0, 0x4A, 0x4A, 0xFF }; // invalid (red)
constexpr Color CLR_NAV       = { 0x4F, 0xC8, 0xE6, 0xFF }; // nav target
constexpr Color CLR_MONSTER   = { 0xE0, 0x5A, 0x5A, 0xFF };
constexpr Color CLR_ITEM      = { 0xE6, 0xC8, 0x4F, 0xFF };
constexpr Color CLR_NPC       = { 0x5A, 0xE0, 0x8A, 0xFF };

// Demo wire context: the app has no real inventory/equipment, so expose fixed
// demo bindings so HUD commands actually translate+encode (the "emit and drop"
// demo). Hp slot 0, Mp slot 1, magic shortcuts 0..2.
class DemoWireContext final : public Protocol::IWireContext {
public:
    int16_t attackActionType(bool /*super*/) const override { return 3; }

    int16_t consumableSlot(Input::UseItemSlot slot) const override
    {
        return (slot == Input::UseItemSlot::Hp) ? 0 : 1;
    }

    bool shortcutBinding(uint8_t shortcutSlot,
                         Protocol::ShortcutBinding& out) const override
    {
        out = Protocol::ShortcutBinding{};
        out.isValid = true;
        out.isMagic = true;
        out.magicId = 10 + shortcutSlot; // demo magic ids
        return true;
    }
};

} // namespace

Game::Game(Core::Application& app)
    : m_app(app)
    , m_btnBack(Systems::UI::makeButton("Back", 12, 12, 80, 28))
{
}

void Game::onEnter()
{
    initWorld();
    m_gameWorld.setGrid(&m_world);
    m_gameWorld.setPlayerId(1);
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
        m_camera.setZoom(CAM_ZOOM);
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
        m_camera.setZoom(CAM_ZOOM);
        m_camera.reset(Vector2{ 15 * TILE_SIZE, 10 * TILE_SIZE });
    }

    // Demo targets around the spawn (only when their tile is walkable, so a
    // real map's walls just skip them).
    const Simulation::GridCoord spawn = m_sim.tilePosition();
    struct DemoEnt { int dx, dy; Simulation::TargetKind k; bool a, p, i; };
    const DemoEnt ents[] = {
        {  8, 0, Simulation::TargetKind::Monster, true, false, false },
        { -4, 2, Simulation::TargetKind::Item,    false, true,  false },
        {  0, -3, Simulation::TargetKind::Npc,    true,  false, true  },
    };
    uint32_t nextId = 10;
    for (const DemoEnt& e : ents)
    {
        const Simulation::GridCoord at{ spawn.x + e.dx, spawn.y + e.dy };
        if (!m_world.isWalkable(at)) continue;
        m_gameWorld.add(Simulation::TargetInfo{
            nextId++, at, e.k, e.a, e.p, e.i });
    }
}

Simulation::GridCoord Game::screenToTile(float sx, float sy) const
{
    // Inverse of the camera render transform: world = (screen - offset)/zoom +
    // origin (raylib GetScreenToWorld2D with offset (0,0)).
    const float wx = m_camera.origin().x + sx / m_camera.zoom();
    const float wy = m_camera.origin().y + sy / m_camera.zoom();
    return Simulation::GridCoord{ (int)(wx / TILE_SIZE),
                                  (int)(wy / TILE_SIZE) };
}

void Game::routeFrame()
{
    m_manualActive = false;

    for (auto it = m_frame.begin(); it != m_frame.end(); ++it)
    {
        const Input::PlayerCommand& cmd = *it;

        if (const auto* mv = std::get_if<Input::PlayerMove>(&cmd))
        {
            // Manual (joystick/keys) movement: suspend nav, drive the sim.
            m_manualActive = true;
            m_sim.handleInput(mv->direction, mv->locomotion);
        }
        else if (const auto* st = std::get_if<Input::PlayerSetTarget>(&cmd))
        {
            if (const auto resolved =
                    Simulation::TargetResolver::resolve(m_gameWorld, *st))
            {
                m_nav.engage(*resolved);
                m_nav.setSuspended(false);
            }
            // unresolvable SetTarget: silently dropped (fail closed)
        }
        else
        {
            // Attack / Cast / UseItem / Toggle: translate → encode → emit-dropped
            // (demonstration; real networking arrives later).
            emitProtocol(cmd);
        }
    }

    if (m_manualActive && !m_prevManualActive)
        m_nav.setSuspended(true);   // manual grab: suspend nav, keep target
    else if (!m_manualActive && m_prevManualActive)
        m_nav.setSuspended(false);  // manual release: resume nav from here
    m_prevManualActive = m_manualActive;
}

void Game::emitProtocol(const Input::PlayerCommand& cmd)
{
    static DemoWireContext wireCtx;
    const std::vector<Protocol::ProtocolCommand> wire =
        Protocol::CommandTranslator::translate(cmd, m_gameWorld, wireCtx);
    for (const auto& wc : wire)
    {
        const std::vector<uint8_t> bytes =
            Protocol::HelbreathPacketEncoder::encode(wc, m_encodeCtx);
        // Demo only: log the size and drop the packet (no server yet).
        TraceLog(LOG_INFO, "Game: emit %zu bytes (type=%u)", bytes.size(),
                 (unsigned)(std::holds_alternative<Protocol::Motion>(wc) ? 1
                            : std::holds_alternative<Protocol::MotionAttack>(wc) ? 2
                            : 3));
    }
}

void Game::update(float dt)
{
    m_timeMs += (uint64_t)(dt * 1000.0f);
    m_encodeCtx.timeMs = (uint32_t)m_timeMs;

    // World snapshot for the producer's tap-to-target classification.
    m_gameWorld.setPlayer(m_sim.tilePosition());

    // Producer: touch + keys → PlayerInputFrame.
    m_frame = m_hud.update(m_app.input().touchFrame(),
                           m_app.input().keyState(),
                           m_gameWorld,
                           [this](float sx, float sy) {
                               return screenToTile(sx, sy);
                           },
                           dt);

    // Route the frame: manual move, nav engage, protocol emits.
    routeFrame();

    // Sim cadence first, then nav consumes a committed-step opportunity.
    float dtMs = dt * 1000.0f;
    m_sim.update(dtMs);

    bool navDrove = false;
    if (m_nav.isEngaged() && !m_manualActive && m_sim.beginStepOpportunity())
    {
        Simulation::NavExecutor::NavResult r =
            m_nav.nextMove(m_gameWorld, m_sim.tilePosition());
        if (r.move)
        {
            m_sim.handleInput(r.move->direction, r.move->locomotion);
            navDrove = true;
        }
        else
        {
            m_sim.releaseInput();
        }
        if (r.action)
            emitProtocol(r.action.value());
    }

    // Nothing drove this frame (manual idle, nav between steps): stand still.
    if (!m_manualActive && !navDrove)
        m_sim.releaseInput();

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

    if (GRIDPLAY_GETENV("GRIDPLAY_PROBE_NOCAM"))
    {
        DrawRectangle(0, 0, (int)LOGICAL_W, (int)LOGICAL_H, MAGENTA);
        if (m_mapLoaded && !m_map.textures().textures.empty())
        {
            const Texture2D& tex = m_map.textures().textures[0];
            DrawTextureRec(tex, Rectangle{ 0, 0, 600, 600 },
                           Vector2{ 340, 60 }, WHITE);
        }
        rlDrawRenderBatchActive();
        static int dbgFrame2 = 0;
        const char* dbgShot2 = GRIDPLAY_GETENV("GRIDPLAY_SHOT");
        if (dbgShot2 && dbgShot2[0] && dbgFrame2 < 150)
        {
            ++dbgFrame2;
            if (dbgFrame2 == 75)
                TakeScreenshot(dbgShot2);
        }
        return;
    }

    if (GRIDPLAY_GETENV("GRIDPLAY_PROBE_RED"))
        DrawRectangle(0, 0, (int)LOGICAL_W, (int)LOGICAL_H, RED);

    m_camera.apply();
    if (m_mapLoaded)
        m_map.draw(m_camera);
    else
        drawGrid();
    drawPlayer();
    drawTargetMarkers();

    if (GRIDPLAY_GETENV("GRIDPLAY_PROBE_CAM_TEX") && m_mapLoaded)
    {
        DrawRectangle(2630, 2600, 600, 600, MAGENTA);
        if (!m_map.textures().textures.empty())
        {
            const Texture2D& tex = m_map.textures().textures[0];
            Rectangle src{ 0, 0, (float)tex.width, (float)tex.height };
            DrawTextureRec(tex, src, Vector2{ 2096, 2376 }, WHITE);
        }
    }

    m_camera.restore();

    m_hud.render();
    drawHud();
    Systems::UI::drawButton(m_btnBack,
                            { 0x33, 0x33, 0x55, 0xFF },
                            { 0x55, 0x55, 0x88, 0xFF },
                            { 0x88, 0xAA, 0xFF, 0xFF });

    // Debug aid: GRIDPLAY_SHOT=/path/shot.png dumps one frame (env only).
    static int dbgFrame = 0;
    const char* dbgShot = GRIDPLAY_GETENV("GRIDPLAY_SHOT");
    if (dbgShot && dbgShot[0] && dbgFrame < 150)
    {
        ++dbgFrame;
        if (dbgFrame == 75)
        {
            rlDrawRenderBatchActive();
            TakeScreenshot(dbgShot);
        }
    }
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

void Game::drawTargetMarkers()
{
    // Demo entity markers (monster/item/NPC) as colored diamonds.
    // The world adapter holds the same entity list; iterate deterministically.
    for (uint32_t id = 10; id < 20; ++id)
    {
        Simulation::TargetInfo ent;
        if (!m_gameWorld.tryGetTarget(id, ent)) break;
        const Color col =
            (ent.kind == Simulation::TargetKind::Monster) ? CLR_MONSTER
            : (ent.kind == Simulation::TargetKind::Item)  ? CLR_ITEM
                                                          : CLR_NPC;
        const Vector2 c{ ent.position.x * TILE_SIZE + TILE_SIZE / 2,
                         ent.position.y * TILE_SIZE + TILE_SIZE / 2 };
        DrawCircleV(c, 6.0f, col);
        DrawCircleLinesV(c, 9.0f, Fade(col, 0.5f));
    }

    // Nav target marker: engaged destination tile.
    if (const auto* t = m_nav.target())
    {
        const Color col = (m_nav.status() == Simulation::GreedyNavigator::Status::Reached)
                              ? CLR_NAV
                              : CLR_SPAWN;
        const Vector2 c{ t->destination.x * TILE_SIZE + TILE_SIZE / 2,
                         t->destination.y * TILE_SIZE + TILE_SIZE / 2 };
        DrawRectangleLinesEx(
            Rectangle{ t->destination.x * TILE_SIZE + 2,
                       t->destination.y * TILE_SIZE + 2,
                       TILE_SIZE - 4, TILE_SIZE - 4 },
            2.0f, col);
        DrawCircleV(c, 5.0f, Fade(col, 0.7f));
    }

    // Active reticle (finger still down on the right band).
    const HUD::MobileControlsHud::View& v = m_hud.view();
    if (v.reticleActive)
    {
        const Simulation::GridCoord tile = m_hud.reticleTile();
        const Color col = v.reticleValid ? CLR_RETICLE : CLR_RETICLE_B;
        DrawRectangleLinesEx(
            Rectangle{ tile.x * TILE_SIZE + 2, tile.y * TILE_SIZE + 2,
                       TILE_SIZE - 4, TILE_SIZE - 4 },
            2.0f, col);
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
    Systems::Rendering::text("Left band: joystick   Right band: target",
                             16, 44, 14, { 0x88, 0x99, 0xAA, 0xFF });

    auto pres = m_sim.presentation(TILE_SIZE);
    const char* loco = (pres.locomotion == Simulation::Locomotion::Running)
                           ? "Running" : "Walking";
    if (!pres.isMoving) loco = "Standing";
    const char* nav = "off";
    if (m_nav.isEngaged())
        nav = m_nav.isSuspended() ? "suspended" : "on";

    Systems::Rendering::text(
        TextFormat("tile  %d,%d    dir  %d    %s", (int)m_sim.tilePosition().x,
                   (int)m_sim.tilePosition().y, (int)pres.facing, loco),
        16, 64, 14, { 0x88, 0x99, 0xAA, 0xFF });
    Systems::Rendering::text(
        TextFormat("nav   %s    atk-target  %s", nav,
                   m_hud.target().valid ? TextFormat("%u", m_hud.target().id)
                                        : "none"),
        16, 84, 14, { 0x88, 0x99, 0xAA, 0xFF });
}

} // namespace Screens