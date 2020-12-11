#include "Ecs.hh"

#include "EcsImpl.hh"

#include <typeindex>

namespace ecs {
    void Entity::Destroy() {
        auto lock = em->tecs.StartTransaction<Tecs::AddRemove>();
        e.Destroy(lock);
    }

    EntityManager::EntityManager() {}

    Entity EntityManager::NewEntity() {
        auto lock = tecs.StartTransaction<Tecs::AddRemove>();
        return Entity(this, lock.NewEntity());
    }

    void EntityManager::DestroyAll() {
        auto lock = tecs.StartTransaction<Tecs::AddRemove>();
        for (auto e : lock.Entities()) {
            e.Destroy(lock);
        }
    }
} // namespace ecs
