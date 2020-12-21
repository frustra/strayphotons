#include "Ecs.hh"

#include "EcsImpl.hh"

#include <typeindex>

namespace ecs {
    std::string ToString(Lock<Read<Name>> lock, Tecs::Entity e) {
        if (lock.Exists(e)) {
            if (e.Has<Name>(lock)) {
                return e.Get<Name>(lock) + "(" + std::to_string(e.id) + ")";
            } else {
                return "Entity(" + std::to_string(e.id) + ")";
            }
        } else {
            return "Entity(NULL)";
        }
    }
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
