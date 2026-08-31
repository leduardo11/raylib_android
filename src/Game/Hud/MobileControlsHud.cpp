#include "Game/Hud/MobileControlsHud.h"

#include "Game/Input/DirectionQuantizer.h"

#include <cmath>

namespace HUD {

inline constexpr float HUD_JOYSTICK_DEAD_ZONE = 0.22f; // = Input::JOYSTICK_DEAD_ZONE
inline constexpr float JOY_RADIUS = 56.0f;
inline constexpr float RING_OFFSET = 84.0f;
inline constexpr float RING_W = 84.0f;
inline constexpr float RING_H = 40.0f;

const Rect* HudLayout::buttonAt(float px, float py) const
{
    const HudLayout& l = get();
    const Rect* rects[] = { &l.menu,    &l.attack, &l.run,    &l.super,
                            &l.stance,  &l.hpBtn,  &l.mpBtn,
                            &l.magic1,  &l.magic2, &l.magic3 };
    for (const Rect* r : rects)
        if (contains(*r, px, py)) return r;
    return nullptr;
}

int HudLayout::windowAt(float px, float py) const
{
    const HudLayout& l = get();
    const Rect* rects[6] = { &l.window0, &l.window1, &l.window2,
                             &l.window3, &l.window4, &l.window5 };
    for (int i = 0; i < 6; ++i)
        if (contains(*rects[i], px, py)) return i;
    return -1;
}

bool MobileControlsHud::anyTouchOver(const Input::TouchFrame& t, const Rect& r)
{
    for (uint16_t i = 0; i < t.count; ++i)
        if (t.points[i].down && contains(r, t.points[i].x, t.points[i].y))
            return true;
    return false;
}

void MobileControlsHud::classifyTile(const Simulation::ITargetWorld& world,
                                     Simulation::GridCoord tile)
{
    m_reticleValid = false;
    m_reticleHasTarget = false;
    m_reticleTargetId = 0;
    m_reticleKind = Simulation::TargetKind::Monster;
    m_reticleVerb = Input::TargetVerb::Move;

    if (!world.isTileValid(tile)) return;

    Simulation::TargetInfo ent;
    if (world.findTargetAt(tile, ent))
    {
        if (world.isPlayerTargetId(ent.id)) return; // self: strict no-op

        m_reticleValid = true;
        m_reticleHasTarget = true;
        m_reticleTargetId = ent.id;
        m_reticleKind = ent.kind;
        switch (ent.kind)
        {
            case Simulation::TargetKind::Monster:
                m_reticleVerb = ent.attackable ? Input::TargetVerb::Attack
                                               : Input::TargetVerb::Move;
                break;
            case Simulation::TargetKind::Item:
                m_reticleVerb = Input::TargetVerb::Pickup;
                break;
            case Simulation::TargetKind::Npc:
                m_reticleVerb = Input::TargetVerb::Interact;
                break;
        }
        return;
    }

    m_reticleValid = true;
    m_reticleVerb = Input::TargetVerb::Move; // ground
}

void MobileControlsHud::pushSetTarget(Input::PlayerInputFrame& frame,
                                      Simulation::GridCoord tile,
                                      uint32_t id, bool hasId,
                                      Input::TargetVerb verb)
{
    frame.push(Input::PlayerSetTarget{ tile, id, hasId, verb });
    if (hasId && verb != Input::TargetVerb::Move)
        m_target = TargetRef{ id, true };
}

void MobileControlsHud::emitAttack(Input::PlayerInputFrame& frame)
{
    frame.push(Input::PlayerAttack{ m_target.valid ? m_target.id : 0, m_superHeld });
}

Input::PlayerInputFrame MobileControlsHud::update(
    const Input::TouchFrame& touches,
    const Input::KeyState& keys,
    const Simulation::ITargetWorld& world,
    const std::function<Simulation::GridCoord(float, float)>& screenToTile,
    float dtSec)
{
    const HudLayout& L = HudLayout::get();

    // ── held-state snapshot (buttons vs gestures) ───────────────────────
    const bool runDown  = anyTouchOver(touches, L.run);
    const bool runtimeDown = anyTouchOver(touches, L.super);
    const bool atkDown  = anyTouchOver(touches, L.attack);
    const bool hpDown   = anyTouchOver(touches, L.hpBtn);
    const bool mpDown   = anyTouchOver(touches, L.mpBtn);

    Input::PlayerInputFrame frame;

    // ── touch edge handling ─────────────────────────────────────────────
    for (uint16_t i = 0; i < touches.count; ++i)
    {
        const Input::TouchPoint& t = touches.points[i];

        if (t.justPressed)
        {
            // ☰ menu is modal while open (§5.5): window pick, else collapse.
            if (m_menuOpen)
            {
                if (contains(L.menu, t.x, t.y))
                {
                    m_menuOpen = false;
                }
                else if (const int widx = L.windowAt(t.x, t.y); widx >= 0)
                {
                    frame.push(Input::PlayerToggle{
                        Input::ToggleKind::Window, true,
                        static_cast<uint16_t>(widx) });
                    m_menuOpen = false;
                }
                else
                {
                    m_menuOpen = false; // outside-tap collapses, press consumed
                }
                continue;
            }

            if (contains(L.menu, t.x, t.y))
            {
                m_menuOpen = true;
                continue;
            }

            // NPC context ring (choose a verb, not confirm a guess).
            if (m_ringOpen)
            {
                bool consumed = false;
                const Rect btns[4] = {
                    { m_ringX - RING_W / 2, m_ringY - RING_OFFSET - RING_H / 2,
                      RING_W, RING_H },                          // N talk
                    { m_ringX + RING_OFFSET - RING_W / 2, m_ringY - RING_H / 2,
                      RING_W, RING_H },                          // E attack
                    { m_ringX - RING_W / 2, m_ringY + RING_OFFSET - RING_H / 2,
                      RING_W, RING_H },                          // S trade
                    { m_ringX - RING_OFFSET - RING_W / 2, m_ringY - RING_H / 2,
                      RING_W, RING_H } };                        // W inspect
                for (int b = 0; b < 4; ++b)
                {
                    if (!contains(btns[b], t.x, t.y)) continue;
                    const Input::TargetVerb verb =
                        (b == 1) ? Input::TargetVerb::Attack
                                 : Input::TargetVerb::Interact;
                    pushSetTarget(frame, m_ringTile, m_ringId, true, verb);
                    m_ringOpen = false;
                    consumed = true;
                    break;
                }
                if (!consumed)
                    m_ringOpen = false; // outside-tap closes the ring
                continue;
            }

            // Buttons: a touch that lands on a button is a command, never a
            // joystick/target action (Rule 1).
            if (contains(L.attack, t.x, t.y))
            {
                m_atkPressPending = true;
                continue;
            }
            if (contains(L.hpBtn, t.x, t.y))
            {
                m_hpAccum = 0.0f;
                frame.push(Input::PlayerUseItem{ Input::UseItemSlot::Hp });
                continue;
            }
            if (contains(L.mpBtn, t.x, t.y))
            {
                m_mpAccum = 0.0f;
                frame.push(Input::PlayerUseItem{ Input::UseItemSlot::Mp });
                continue;
            }
            if (contains(L.magic1, t.x, t.y))
            {
                frame.push(Input::PlayerUseItem{ Input::UseItemSlot::Shortcut1 });
                continue;
            }
            if (contains(L.magic2, t.x, t.y))
            {
                frame.push(Input::PlayerUseItem{ Input::UseItemSlot::Shortcut2 });
                continue;
            }
            if (contains(L.magic3, t.x, t.y))
            {
                frame.push(Input::PlayerUseItem{ Input::UseItemSlot::Shortcut3 });
                continue;
            }
            if (contains(L.stance, t.x, t.y))
            {
                m_stanceOn = !m_stanceOn;
                frame.push(Input::PlayerToggle{
                    Input::ToggleKind::Stance, m_stanceOn, 0 });
                continue;
            }
            if (contains(L.run, t.x, t.y))
            {
                m_runPersistent = !m_runPersistent;
                frame.push(Input::PlayerToggle{
                    Input::ToggleKind::Run, m_runPersistent, 0 });
                continue;
            }
            if (contains(L.super, t.x, t.y))
                continue; // momentary modifier: held state only

            // Gesture zones (§3 finger-slots): left band floating joystick,
            // right band target reticle (fires on release).
            if (t.x < HUD::JOYSTICK_ZONE_W)
            {
                m_joyId = t.id;
                m_joyOriginX = m_joyX = t.x;
                m_joyOriginY = m_joyY = t.y;
                m_joyRad = 0.0f;
            }
            else
            {
                m_reticleId = t.id;
                m_reticleX = t.x;
                m_reticleY = t.y;
                m_reticleTile = screenToTile(t.x, t.y);
                classifyTile(world, m_reticleTile);
            }
        }

        if (t.justReleased)
        {
            if (t.id == m_joyId)
            {
                m_joyId = -1;
                m_joyRad = 0.0f;
            }
            else if (t.id == m_reticleId)
            {
                if (m_reticleValid)
                {
                    if (m_reticleHasTarget &&
                        m_reticleKind == Simulation::TargetKind::Npc)
                    {
                        m_ringOpen = true;
                        m_ringX = m_reticleX;
                        m_ringY = m_reticleY;
                        m_ringTile = m_reticleTile;
                        m_ringId = m_reticleTargetId;
                    }
                    else
                    {
                        pushSetTarget(frame, m_reticleTile,
                                      m_reticleTargetId, m_reticleHasTarget,
                                      m_reticleVerb);
                    }
                }
                m_reticleId = -1;
            }
        }
    }

    // ── joystick vector + reticle drag (continuous while held) ──────────
    if (m_joyId >= 0)
    {
        if (const Input::TouchPoint* jp = touches.find(m_joyId))
        {
            m_joyX = jp->x;
            m_joyY = jp->y;
        }
        const float dx = m_joyX - m_joyOriginX;
        const float dy = m_joyY - m_joyOriginY;
        const float mag = sqrtf(dx * dx + dy * dy);
        m_joyRad = mag / JOY_RADIUS;

        const bool runOn = runDown || m_runPersistent;
        if (m_joyRad >= HUD_JOYSTICK_DEAD_ZONE)
        {
            const float s = (mag > JOY_RADIUS) ? JOY_RADIUS / mag : 1.0f;
            const Simulation::Direction dir = Input::DirectionQuantizer::fromVector(
                (dx * s) / JOY_RADIUS, (dy * s) / JOY_RADIUS);
            frame.push(Input::PlayerMove{
                dir,
                runOn ? Simulation::Locomotion::Running
                      : Simulation::Locomotion::Walking });
        }
    }
    if (m_reticleId >= 0)
    {
        if (const Input::TouchPoint* rp = touches.find(m_reticleId))
        {
            m_reticleX = rp->x;
            m_reticleY = rp->y;
            m_reticleTile = screenToTile(rp->x, rp->y);
            classifyTile(world, m_reticleTile); // reticle follows the finger
        }
    }

    // ── momentary modifier edges ────────────────────────────────────────
    if (runDown != m_runHeldPrev)
    {
        m_runHeld = runDown;
        frame.push(Input::PlayerToggle{
            Input::ToggleKind::Run, m_runHeld, 0 });
    }
    m_runHeldPrev = runDown;
    m_superHeld = runtimeDown;

    // ── attack button: immediate on press, then repeat at the gate ──────
    if (m_atkPressPending)
    {
        emitAttack(frame);
        m_atkAccum = 0.0f;
        m_atkPressPending = false;
    }
    if (atkDown)
    {
        m_atkAccum += dtSec;
        while (m_atkAccum >= ATTACK_REPEAT_S)
        {
            m_atkAccum -= ATTACK_REPEAT_S;
            emitAttack(frame);
        }
    }
    else
    {
        m_atkAccum = 0.0f;
    }

    // ── potions: touch buttons + keyboard Insert/Delete hold-repeat ─────
    m_hpAccum = hpDown ? m_hpAccum + dtSec : 0.0f;
    m_mpAccum = mpDown ? m_mpAccum + dtSec : 0.0f;
    if (hpDown)
    {
        while (m_hpAccum >= POTION_REPEAT_S)
        {
            m_hpAccum -= POTION_REPEAT_S;
            frame.push(Input::PlayerUseItem{ Input::UseItemSlot::Hp });
        }
    }
    if (mpDown)
    {
        while (m_mpAccum >= POTION_REPEAT_S)
        {
            m_mpAccum -= POTION_REPEAT_S;
            frame.push(Input::PlayerUseItem{ Input::UseItemSlot::Mp });
        }
    }
    if (keys.hpInsertHeld != m_keyHpHold)
    {
        m_keyHpHold = keys.hpInsertHeld;
        m_keyHpAccum = m_keyHpHold ? 0.0f : 0.0f; // fresh on each press
        frame.push(Input::PlayerUseItem{ Input::UseItemSlot::Hp });
    }
    else if (m_keyHpHold)
    {
        m_keyHpAccum += dtSec;
        while (m_keyHpAccum >= POTION_REPEAT_S)
        {
            m_keyHpAccum -= POTION_REPEAT_S;
            frame.push(Input::PlayerUseItem{ Input::UseItemSlot::Hp });
        }
    }
    if (keys.mpDeleteHeld != m_keyMpHold)
    {
        m_keyMpHold = keys.mpDeleteHeld;
        frame.push(Input::PlayerUseItem{ Input::UseItemSlot::Mp });
    }
    else if (m_keyMpHold)
    {
        m_keyMpAccum += dtSec;
        while (m_keyMpAccum >= POTION_REPEAT_S)
        {
            m_keyMpAccum -= POTION_REPEAT_S;
            frame.push(Input::PlayerUseItem{ Input::UseItemSlot::Mp });
        }
    }

    // ── keyboard parity (§2.1): movement + one-shot keys ────────────────
    if (m_joyId < 0)
    {
        const float dx = (keys.moveRight ? 1.0f : 0.0f) -
                         (keys.moveLeft  ? 1.0f : 0.0f);
        const float dy = (keys.moveDown  ? 1.0f : 0.0f) -
                         (keys.moveUp    ? 1.0f : 0.0f);
        if (dx != 0.0f || dy != 0.0f)
        {
            const float len = sqrtf(dx * dx + dy * dy);
            const Simulation::Direction dir =
                Input::DirectionQuantizer::fromVector(dx / len, dy / len);
            const bool runOn = keys.shiftHeld || m_runPersistent;
            frame.push(Input::PlayerMove{
                dir,
                runOn ? Simulation::Locomotion::Running
                      : Simulation::Locomotion::Walking });
        }
    }

    if (keys.ctrlRRun)
    {
        m_runPersistent = !m_runPersistent;
        frame.push(Input::PlayerToggle{
            Input::ToggleKind::Run, m_runPersistent, 0 });
    }
    if (keys.tabStance)
    {
        m_stanceOn = !m_stanceOn;
        frame.push(Input::PlayerToggle{ Input::ToggleKind::Stance, m_stanceOn, 0 });
    }
    if (keys.homeSafe)
        frame.push(Input::PlayerToggle{ Input::ToggleKind::SafeAttack, true, 0 });
    if (keys.ctrlAForce)
        frame.push(Input::PlayerToggle{ Input::ToggleKind::ForceAttack, true, 0 });
    if (keys.pageUp)
        frame.push(Input::PlayerCast{ 0, Input::CastKind::SpecAbility });
    if (keys.f2)
        frame.push(Input::PlayerUseItem{ Input::UseItemSlot::Shortcut1 });
    if (keys.f3)
        frame.push(Input::PlayerUseItem{ Input::UseItemSlot::Shortcut2 });
    if (keys.f4)
        frame.push(Input::PlayerUseItem{ Input::UseItemSlot::Shortcut3 });
    const bool winKeys[6] = { keys.f5, keys.f6, keys.f7, keys.f8, keys.f9,
                              keys.f10 };
    for (int w = 0; w < 6; ++w)
        if (winKeys[w])
            frame.push(Input::PlayerToggle{ Input::ToggleKind::Window, true,
                                            static_cast<uint16_t>(w) });

    // ── view snapshot for the render / screen side ──────────────────────
    m_view = View{};
    m_view.joystickActive = m_joyId >= 0;
    m_view.joyOriginX = m_joyOriginX;
    m_view.joyOriginY = m_joyOriginY;
    m_view.joyX = m_joyX;
    m_view.joyY = m_joyY;
    m_view.reticleActive = m_reticleId >= 0;
    m_view.reticleX = m_reticleX;
    m_view.reticleY = m_reticleY;
    m_view.reticleValid = m_reticleValid;
    m_view.reticleKind = m_reticleKind;
    m_view.reticleHasTarget = m_reticleHasTarget;
    m_view.ringOpen = m_ringOpen;
    m_view.ringX = m_ringX;
    m_view.ringY = m_ringY;
    m_view.menuOpen = m_menuOpen;
    m_view.runHeld = m_runHeld || m_runPersistent;
    m_view.superHeld = m_superHeld;
    m_view.stanceOn = m_stanceOn;
    m_view.attackEnabled = true;
    m_view.attackPressed = atkDown || m_atkPressPending;

    return frame;
}

} // namespace HUD