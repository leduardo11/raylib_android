#pragma once

#include "raylib.h"

namespace Game { class EntityManager; }

namespace Game {

struct Logic {
    static void update(EntityManager& mgr, float dt);
    static bool checkCollision(const class Entity& a, const class Entity& b);
};

} // namespace Game
