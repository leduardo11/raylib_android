#include "UI.h"

namespace Systems {

Button UI::makeButton(const char* text, float cx, float y, float w, float h)
{
    Button btn;
    btn.bounds = { cx - w / 2, y, w, h };
    btn.text = text;
    return btn;
}

void UI::updateButton(Button& btn, Vector2 pointer, bool pointerPressed)
{
    btn.hovered = CheckCollisionPointRec(pointer, btn.bounds);
    btn.clicked = btn.hovered && pointerPressed;
}

void UI::drawButton(const Button& btn, Color normal, Color hover, Color outline)
{
    Color c = btn.hovered ? hover : normal;
    DrawRectangleRec(btn.bounds, c);
    DrawRectangleLinesEx(btn.bounds, 2, outline);

    int textW = MeasureText(btn.text, 24);
    DrawText(btn.text,
             (int)(btn.bounds.x + btn.bounds.width / 2 - textW / 2),
             (int)(btn.bounds.y + btn.bounds.height / 2 - 12),
             24, WHITE);
}

} // namespace Systems
