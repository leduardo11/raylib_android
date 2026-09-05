#pragma once

// MobileControlsHud: the touch + keyboard HUD — a *producer* of
// PlayerInputFrame commands, per the approved v2 layout.
//
// Its only job is touch/key → gesture/button interpretation → PlayerInputFrame.
// It knows nothing about: ENet, packet structs, ProtocolCommand, wire ids,
// packet encoding, navigation algorithms, attack range, collision, or gameplay
// execution. The only world knowledge it has is world OBSERVATION through
// ITargetWorld (does an entity sit at this tile; is it a monster/item/NPC) —
// the same observational seam the resolver uses, used here only to classify a
// tap like the reference client's cursor does.
//
// This file is deliberately raylib-free so the producer logic is unit-testable
// in gridplay_tests. Rendering lives in MobileControlsHud.cpp.

#include "Game/Input/KeyState.h"
#include "Game/Input/PlayerCommand.h"
#include "Game/Input/PlayerInputFrame.h"
#include "Game/Input/TouchFrame.h"
#include "Game/Simulation/TargetWorld.h"

#include <cstdint>
#include <functional>

namespace HUD {

// Logical 1280x720 canvas (same logical space as the game screen visuals).
inline constexpr float LOGICAL_W = 1280.0f;
inline constexpr float LOGICAL_H = 720.0f;

// Finger-slot zones (§3): left band = joystick, right band = target reticle.
// Buttons win over both (Rule 1: a touch on a button is a command, never a
// joystick/target action).
inline constexpr float JOYSTICK_ZONE_W = 0.70f * LOGICAL_W;

// Repeat cadences (reference parity: potions at the original 500ms auto-repeat
// gate; the attack gate is a placeholder until the real swing timing is wired).
inline constexpr float ATTACK_REPEAT_S   = 0.42f;
inline constexpr float POTION_REPEAT_S   = 0.50f;

// Window indices exposed by the ☰ menu (DialogBox_HudPanel::TOGGLE_BUTTONS).
enum class HudWindow : uint8_t {
    Character = 0, Inventory = 1, Magics = 2,
    Skills = 3, ChatLog = 4, SystemMenu = 5,
    Count = 6
};

struct Rect {
    float x, y, w, h;
};

inline bool contains(const Rect& r, float px, float py)
{
    return px >= r.x && px <= r.x + r.w && py >= r.y && py <= r.y + r.h;
}

// The approved layout, in logical coordinates (§3).
struct HudLayout {
    Rect menu     { 1172.0f, 12.0f, 96.0f, 44.0f };
    Rect run      { 1084.0f, 12.0f, 80.0f, 44.0f };
    Rect super    { 1090.0f, 480.0f, 96.0f, 52.0f };
    Rect attack   { 1090.0f, 544.0f, 124.0f, 124.0f };
    Rect stance   { 1222.0f, 544.0f, 46.0f, 46.0f };
    Rect hpBtn    { 836.0f, 620.0f, 68.0f, 52.0f };
    Rect mpBtn    { 908.0f, 620.0f, 68.0f, 52.0f };
    Rect magic1   { 980.0f, 620.0f, 52.0f, 52.0f };
    Rect magic2   { 1036.0f, 620.0f, 52.0f, 52.0f };
    Rect magic3   { 1096.0f, 620.0f, 52.0f, 52.0f };
    // ☰ menu fan-out (2 rows x 3 windows), anchored under the menu button.
    Rect window0  { 1030.0f, 64.0f, 140.0f, 38.0f };
    Rect window1  { 1030.0f, 106.0f, 140.0f, 38.0f };
    Rect window2  { 1030.0f, 148.0f, 140.0f, 38.0f };
    Rect window3  { 1030.0f, 190.0f, 140.0f, 38.0f };
    Rect window4  { 1030.0f, 232.0f, 140.0f, 38.0f };
    Rect window5  { 1030.0f, 274.0f, 140.0f, 38.0f };

    const Rect* buttonAt(float px, float py) const; // non-window buttons
    int windowAt(float px, float py) const;         // -1 when none

    static const HudLayout& get()
    {
        static const HudLayout l; // constexpr construction, single instance
        return l;
    }
};

class MobileControlsHud {
public:
    // Two contexts the producer can legally name the world in while staying
    // observational: the last entity the player engaged (ATK button target)
    // and the reticle/ring view for drawing.
    struct TargetRef {
        uint32_t id = 0;
        bool valid = false;
    };

    // Snapshot of HUD visuals, filled every update for mobile/screen sides.
    struct View {
        bool joystickActive = false;
        float joyOriginX = 0.0f, joyOriginY = 0.0f;
        float joyX = 0.0f, joyY = 0.0f;

        bool reticleActive = false;
        float reticleX = 0.0f, reticleY = 0.0f;
        bool reticleValid = false;         // tile targetable / entity found
        Simulation::TargetKind reticleKind = Simulation::TargetKind::Monster;
        bool reticleHasTarget = false;     // an entity (not ground) under the tile

        bool ringOpen = false;
        float ringX = 0.0f, ringY = 0.0f;

        bool menuOpen = false;
        bool runHeld = false;
        bool superHeld = false;
        bool stanceOn = false;

        bool attackEnabled = false;
        bool attackPressed = false;
    };

    // Producer edge. `screenToTile` converts a screen point to its world tile
    // (the caller owns the camera projection). `dtSec` advances hold-repeat
    // timers (attack / potions). Fully deterministic and raylib-free.
    Input::PlayerInputFrame update(
        const Input::TouchFrame& touches,
        const Input::KeyState& keys,
        const Simulation::ITargetWorld& world,
        const std::function<Simulation::GridCoord(float, float)>& screenToTile,
        float dtSec);

    const View& view() const { return m_view; }
    const TargetRef& target() const { return m_target; }
    Simulation::GridCoord reticleTile() const { return m_reticleTile; }
    Simulation::GridCoord ringTile() const { return m_ringTile; }

    // Screen-space drawing (raylib; called after camera restore).
    void render() const;

private:
    // ── interaction state ────────────────────────────────────────────────
    int  m_joyId = -1;
    float m_joyOriginX = 0.0f, m_joyOriginY = 0.0f;
    float m_joyX = 0.0f, m_joyY = 0.0f, m_joyRad = 0.0f;

    int  m_reticleId = -1;
    float m_reticleX = 0.0f, m_reticleY = 0.0f;
    Simulation::GridCoord m_reticleTile{ -1, -1 };
    bool m_reticleValid = false;
    bool m_reticleHasTarget = false;
    uint32_t m_reticleTargetId = 0;
    Simulation::TargetKind m_reticleKind = Simulation::TargetKind::Monster;
    Input::TargetVerb m_reticleVerb = Input::TargetVerb::Move;

    bool m_ringOpen = false;
    float m_ringX = 0.0f, m_ringY = 0.0f;
    Simulation::GridCoord m_ringTile{ -1, -1 };
    uint32_t m_ringId = 0;

    bool m_menuOpen = false;
    bool m_superHeld = false;
    bool m_stanceOn = false;
    bool m_runPersistent = false;

    TargetRef m_target;

    // hold-repeat accumulators
    bool  m_atkPressPending = false;
    float m_atkAccum = 0.0f;
    bool  m_hpDown = false, m_mpDown = false;
    float m_hpAccum = 0.0f, m_mpAccum = 0.0f;
    bool  m_keyHpHold = false, m_keyMpHold = false;
    float m_keyHpAccum = 0.0f, m_keyMpAccum = 0.0f;
    bool  m_atkBtnAllocTouch = false;

    View m_view;

    // helpers
    static bool anyTouchOver(const Input::TouchFrame& t, const Rect& r);
    void classifyTile(const Simulation::ITargetWorld& world,
                      Simulation::GridCoord tile);
    void pushSetTarget(Input::PlayerInputFrame& frame,
                       Simulation::GridCoord tile,
                       uint32_t id, bool hasId, Input::TargetVerb verb);
    void emitAttack(Input::PlayerInputFrame& frame);
};

} // namespace HUD