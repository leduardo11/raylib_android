#include "Rendering.h"

namespace Systems {

void Rendering::clear(Color c)
{
    ClearBackground(c);
}

void Rendering::rect(Rectangle r, Color fill, Color outline, float thickness)
{
    DrawRectangleRec(r, fill);
    if (thickness > 0.0f)
        DrawRectangleLinesEx(r, thickness, outline);
}

void Rendering::text(const char* txt, float x, float y, int size, Color c)
{
    DrawText(txt, (int)x, (int)y, size, c);
}

void Rendering::textCentered(const char* txt, float cx, float y, int size, Color c)
{
    float w = (float)MeasureText(txt, size);
    DrawText(txt, (int)(cx - w / 2), (int)y, size, c);
}

int Rendering::textWidth(const char* txt, int size)
{
    return MeasureText(txt, size);
}

} // namespace Systems
