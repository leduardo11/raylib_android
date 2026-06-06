#pragma once

#include "raylib.h"

namespace Systems {

struct Rendering {
    static void clear(Color c);

    static void rect(Rectangle r, Color fill, Color outline, float thickness);
    static void text(const char* txt, float x, float y, int size, Color c);
    static void textCentered(const char* txt, float cx, float y, int size, Color c);

    static int  textWidth(const char* txt, int size);
};

} // namespace Systems
