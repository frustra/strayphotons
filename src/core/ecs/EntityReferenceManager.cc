#include "EntityReferenceManager.hh"

#include "ecs/EcsImpl.hh"

#include <mutex>
#include <shared_mutex>

namespace ecs {
    EntityReferenceManager GEntityRefs;

    EntityRef EntityReferenceManager::Get(const Name &name) {
        std::shared_lock lock(mutex);

        auto result = references.emplace(name, EntityRef{name, Entity()});
        return result.first->second;
    }

    EntityRef EntityReferenceManager::Get(std::string_view scene, std::string_view entity) {
        Name name(scene, entity);
        return Get(name);
    }

    void EntityReferenceManager::Set(const Name &name, const Entity &ent) {
        std::unique_lock lock(mutex);

        auto it = references.find(name);
        if (it != references.end()) {
            it->second.Set(ent);
        } else {
            references.emplace(name, EntityRef{name, ent});
        }
    }
} // namespace ecs
