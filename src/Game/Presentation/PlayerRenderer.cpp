#include "PlayerRenderer.h"

namespace Presentation {

void PlayerRenderer::draw(const PlayerPresentationState& s, float tileSize)
{
    drawGlowRing(s, tileSize);
    drawPlayerShape(s, tileSize);
    drawDirectionArrow(Vector2{ s.position.x, s.position.y },
                       s.facing, tileSize);
}

void PlayerRenderer::drawPlayerShape(const PlayerPresentationState& s,
                                     float tileSize)
{
    float size = tileSize * 0.62f;
    Color body = (s.locomotion == Simulation::Locomotion::Running)
                     ? (Color){ 0x3A, 0xC8, 0x5A, 0xFF }
                     : (Color){ 0x2E, 0xB8, 0x4E, 0xFF };

    Rectangle r{ s.position.x - size / 2.0f,
                 s.position.y - size / 2.0f,
                 size, size };
    DrawRectangleRec(r, body);
    DrawRectangleLinesEx(r, 2.0f, (Color){ 0x9F, 0xF0, 0xB4, 0xFF });
}

void PlayerRenderer::drawDirectionArrow(Vector2 center,
                                        Simulation::Direction facing,
                                        float tileSize)
{
    auto off = Simulation::directionOffset(facing);
    float len = tileSize * 0.38f;
    Vector2 tip{ center.x + off.x * len, center.y + off.y * len };
    DrawLineEx(center, tip, 4.0f, (Color){ 0xE0, 0xFF, 0xEA, 0xFF });
}

void PlayerRenderer::drawGlowRing(const PlayerPresentationState& s,
                                  float tileSize)
{
    float radius = tileSize * 0.40f;
    Color glow = (s.locomotion == Simulation::Locomotion::Running)
                     ? (Color){ 0x5A, 0xD8, 0x7A, 0x55 }
                     : (Color){ 0x7A, 0xE8, 0x9A, 0x44 };
    DrawRing((Vector2){ s.position.x, s.position.y }, radius * 0.6f,
             radius, 0, 360, 0, glow);
}

} // namespace Presentation