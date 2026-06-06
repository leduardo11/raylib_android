#pragma once

#include "raylib.h"

struct TransformComponent {
    Vector2 position{ 0, 0 };
    Vector2 scale{ 1, 1 };
    float   rotation{ 0.0f };
};

struct RenderComponent {
    Color   color{ 255, 255, 255, 255 };
    Vector2 size{ 32, 32 };
    bool    visible{ true };
};

struct PhysicsComponent {
    Vector2 velocity{ 0, 0 };
    Vector2 acceleration{ 0, 0 };
    float   mass{ 1.0f };
    bool    solid{ true };
};
