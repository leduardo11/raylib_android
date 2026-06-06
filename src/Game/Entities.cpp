#include "Entities.h"
#include "Components.h"

namespace Game {

Entity* EntityManager::create()
{
    Entity e;
    e.id = m_nextId++;
    e.transform = new TransformComponent();
    e.render    = new RenderComponent();
    e.physics   = new PhysicsComponent();
    m_entities.push_back(e);
    return &m_entities.back();
}

void EntityManager::destroy(Entity* e)
{
    if (!e) return;
    delete e->transform;
    delete e->render;
    delete e->physics;

    for (auto it = m_entities.begin(); it != m_entities.end(); ++it)
    {
        if (it->id == e->id)
        {
            m_entities.erase(it);
            break;
        }
    }
}

void EntityManager::clear()
{
    for (auto& e : m_entities)
    {
        delete e.transform;
        delete e.render;
        delete e.physics;
    }
    m_entities.clear();
    m_nextId = 1;
}

Entity* EntityManager::get(uint64_t id)
{
    for (auto& e : m_entities)
    {
        if (e.id == id)
            return &e;
    }
    return nullptr;
}

} // namespace Game
