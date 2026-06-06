#pragma once

#include "raylib.h"

namespace Systems {

struct Button {
    Rectangle bounds;
    const char* text;
    bool hovered = false;
    bool clicked = false;
};

struct UI {
    static void updateButton(Button& btn, Vector2 pointer, bool pointerPressed);
    static void drawButton(const Button& btn, Color normal, Color hover, Color outline);

    static Button makeButton(const char* text, float cx, float y, float w, float h);
};

} // namespace Systems
