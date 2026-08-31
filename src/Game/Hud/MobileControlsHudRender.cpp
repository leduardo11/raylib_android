// MobileControlsHudRender.cpp: screen-space drawing for the HUD, kept out of
// MobileControlsHud.cpp so the producer logic in the tests target stays free
// of raylib rendering dependencies. Called by the Game screen after camera
// restore.

#include "Game/Hud/MobileControlsHud.h"

#include "Systems/Rendering.h"
#include "raylib.h"

namespace HUD {

namespace {

constexpr float JOY_RADIUS  = 56.0f;
constexpr float RING_OFFSET = 84.0f;
constexpr float RING_W = 84.0f;
constexpr float RING_H = 40.0f;

void drawButtonRect(const Rect& r, const char* label, Color fill,
                    Color outline, bool highlighted)
{
    const Color hi = highlighted ? Color{ 0x88, 0xAA, 0xFF, 0xFF } : outline;
    DrawRectangle((int)r.x, (int)r.y, (int)r.w, (int)r.h, fill);
    DrawRectangleLinesEx(Rectangle{ r.x, r.y, r.w, r.h }, 2.0f, hi);
    Systems::Rendering::textCentered(label, r.x + r.w / 2, r.y + r.h / 2 - 8,
                                     14, { 0xFC, 0xE9, 0xB0, 0xFF });
}

} // namespace

void MobileControlsHud::render() const
{
    const HudLayout& L = HudLayout::get();
    const View& v = m_view;

    // Bottom gauges (HP / MP / SP / EXP) — read-only, always on.
    const Color gaugeBar  = { 0x1A, 0x20, 0x30, 0xFF };
    const Color gaugeFrame = { 0x3A, 0x42, 0x55, 0xFF };
    const Color hpColor = { 0xB8, 0x38, 0x38, 0xFF };
    const Color mpColor = { 0x38, 0x78, 0xB8, 0xFF };
    const Color spColor = { 0xC8, 0xA8, 0x30, 0xFF };
    const float gw = 282.0f, gy = 700.0f;
    const struct { const char* name; Color c; } gauges[4] = {
        { "HP", hpColor }, { "MP", mpColor },
        { "SP", spColor }, { "EXP", { 0x88, 0x88, 0xAA, 0xFF } } };
    Systems::Rendering::text("HP", 10, 692, 12, hpColor);
    Systems::Rendering::text("MP", 10, 706, 12, mpColor);
    for (int g = 0; g < 4; ++g)
    {
        const float x = (float)(16 + g * (int)(gw + 8));
        DrawRectangle((int)x, (int)gy, (int)gw, 8, gaugeBar);
        DrawRectangleLinesEx(Rectangle{ x, gy, gw, 8 }, 1.0f, gaugeFrame);
        DrawRectangle((int)x + 1, (int)gy + 1,
                      (int)((gw - 2) * (g == 0 ? 0.62f : g == 1 ? 0.45f : 0.80f)),
                      6, gauges[g].c);
        Systems::Rendering::text(
            TextFormat("%s 62/100", gauges[g].name), x + 2, gy + 9, 10,
            { 0x88, 0x99, 0xAA, 0xFF });
    }

    // Magic shortcut slots (F2/F3/F4 parity).
    const Color slotFill = { 0x24, 0x2E, 0x40, 0xFF };
    drawButtonRect(L.magic1, "F2", slotFill, { 0x44, 0x55, 0x77, 0xFF }, false);
    drawButtonRect(L.magic2, "F3", slotFill, { 0x44, 0x55, 0x77, 0xFF }, false);
    drawButtonRect(L.magic3, "F4", slotFill, { 0x44, 0x55, 0x77, 0xFF }, false);

    drawButtonRect(L.hpBtn, "HP", { 0x50, 0x22, 0x22, 0xFF },
                   { 0x88, 0x44, 0x44, 0xFF }, false);
    drawButtonRect(L.mpBtn, "MP", { 0x22, 0x40, 0x50, 0xFF },
                   { 0x44, 0x66, 0x88, 0xFF }, false);

    // Action cluster: RUN / SUPER momentary + STANCE toggle + ATK.
    const char* runLabel = v.runHeld ? "RUN ON" : "RUN OFF";
    drawButtonRect(L.run, runLabel,
                   v.runHeld ? Color{ 0x2E, 0x7D, 0x32, 0xFF }
                             : Color{ 0x2C, 0x38, 0x24, 0xFF },
                   v.runHeld ? Color{ 0x8A, 0xE0, 0x8A, 0xFF }
                             : Color{ 0x55, 0x77, 0x55, 0xFF },
                   v.runHeld);
    drawButtonRect(L.super, "SUPER", { 0x3A, 0x26, 0x18, 0xFF },
                   v.superHeld ? Color{ 0xFF, 0xC0, 0x60, 0xFF }
                               : Color{ 0x88, 0x66, 0x44, 0xFF },
                   v.superHeld);
    drawButtonRect(L.stance, "C", { 0x24, 0x2E, 0x40, 0xFF },
                   v.stanceOn ? Color{ 0x6A, 0xB8, 0x8A, 0xFF }
                              : Color{ 0x44, 0x55, 0x77, 0xFF },
                   v.stanceOn);

    const Color atkFill = v.attackEnabled
        ? (v.attackPressed ? Color{ 0x88, 0x44, 0x30, 0xFF }
                           : Color{ 0x66, 0x2A, 0x20, 0xFF })
        : Color{ 0x2A, 0x2A, 0x38, 0xFF };
    const Color atkOutline = v.attackEnabled
        ? (v.attackPressed ? Color{ 0xFF, 0xC0, 0x60, 0xFF }
                           : Color{ 0xCF, 0x88, 0x60, 0xFF })
        : Color{ 0x55, 0x55, 0x66, 0xFF };
    drawButtonRect(L.attack, "ATK", atkFill, atkOutline, v.attackPressed);

    // ☰ MENU (single expandable entry point, §5.5).
    drawButtonRect(L.menu, v.menuOpen ? "☰▼" : "☰", { 0x30, 0x30, 0x48, 0xFF },
                   v.menuOpen ? Color{ 0x88, 0xAA, 0xFF, 0xFF }
                              : Color{ 0x55, 0x55, 0x88, 0xFF },
                   v.menuOpen);
    if (v.menuOpen)
    {
        const char* labels[6] = { "Character", "Inventory", "Magics",
                                  "Skills",    "Chat Log",  "System" };
        const Rect wins[6] = { L.window0, L.window1, L.window2,
                               L.window3, L.window4, L.window5 };
        for (int w = 0; w < 6; ++w)
            drawButtonRect(wins[w], labels[w], { 0x24, 0x2E, 0x40, 0xFF },
                           { 0x55, 0x66, 0x88, 0xFF }, false);
    }

    // Floating joystick.
    if (v.joystickActive)
    {
        DrawCircleV(Vector2{ v.joyOriginX, v.joyOriginY }, JOY_RADIUS,
                    Fade({ 0x2A, 0x35, 0x46, 0xFF }, 0.55f));
        DrawCircleLinesV(Vector2{ v.joyOriginX, v.joyOriginY }, JOY_RADIUS,
                         Fade({ 0x8A, 0xA0, 0xC0, 0xFF }, 0.6f));
        DrawCircleV(Vector2{ v.joyX, v.joyY }, 18.0f,
                    Fade({ 0x6A, 0xE0, 0x8A, 0xFF }, 0.7f));
    }

    // NPC context ring.
    if (v.ringOpen)
    {
        const char* labels[4] = { "Talk", "Attack", "Trade", "Inspect" };
        const Rect btns[4] = {
            { v.ringX - RING_W / 2, v.ringY - RING_OFFSET - RING_H / 2,
              RING_W, RING_H },
            { v.ringX + RING_OFFSET - RING_W / 2, v.ringY - RING_H / 2,
              RING_W, RING_H },
            { v.ringX - RING_W / 2, v.ringY + RING_OFFSET - RING_H / 2,
              RING_W, RING_H },
            { v.ringX - RING_OFFSET - RING_W / 2, v.ringY - RING_H / 2,
              RING_W, RING_H } };
        DrawCircleLinesV(Vector2{ v.ringX, v.ringY }, RING_OFFSET + 4.0f,
                         Fade({ 0x6A, 0xE0, 0x8A, 0xFF }, 0.5f));
        for (int b = 0; b < 4; ++b)
            drawButtonRect(btns[b], labels[b], { 0x28, 0x30, 0x44, 0xFF },
                           { 0x6A, 0x9A, 0xC6, 0xFF }, false);
    }
}

} // namespace HUD