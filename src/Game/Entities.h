#pragma once

#include <vector>
#include <cstdint>

struct TransformComponent;
struct RenderComponent;
struct PhysicsComponent;

namespace Game {

struct Entity {
    uint64_t id;
    TransformComponent* transform = nullptr;
    RenderComponent*    render    = nullptr;
    PhysicsComponent*   physics   = nullptr;
};

class EntityManager {
public:
    Entity* create();
    void    destroy(Entity* e);
    void    clear();
    Entity* get(uint64_t id);

    const std::vector<Entity>& all() const { return m_entities; }
          std::vector<Entity>& all()       { return m_entities; }

private:
    std::vector<Entity> m_entities;
    uint64_t m_nextId = 1;
};

} // namespace Game
