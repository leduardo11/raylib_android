#include "Logic.h"
#include "Entities.h"
#include "Components.h"

namespace Game {

void Logic::update(EntityManager& mgr, float dt)
{
    for (auto& e : mgr.all())
    {
        if (!e.physics || !e.transform) continue;

        // Simple Euler integration
        e.physics->velocity.x += e.physics->acceleration.x * dt;
        e.physics->velocity.y += e.physics->acceleration.y * dt;

        e.transform->position.x += e.physics->velocity.x * dt;
        e.transform->position.y += e.physics->velocity.y * dt;
    }
}

bool Logic::checkCollision(const Entity& a, const Entity& b)
{
    if (!a.transform || !b.transform || !a.render || !b.render)
        return false;

    Rectangle ra = { a.transform->position.x, a.transform->position.y,
                     a.render->size.x, a.render->size.y };
    Rectangle rb = { b.transform->position.x, b.transform->position.y,
                     b.render->size.x, b.render->size.y };
    return CheckCollisionRecs(ra, rb);
}

} // namespace Game
