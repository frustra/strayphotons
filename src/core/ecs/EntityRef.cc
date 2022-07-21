#include "EntityRef.hh"

#include "ecs/EcsImpl.hh"
#include "ecs/EntityReferenceManager.hh"

#include <atomic>

namespace ecs {
    EntityRef::Ref::Ref(const Entity &ent) {
        if (!ent) return;

        if (Tecs::IdentifierFromGeneration(ent.generation) == ecs::World.GetInstanceId()) {
            liveEntity = ent;
        } else {
            stagingEntity = ent;
        }
    }

    EntityRef::EntityRef(const Entity &ent) {
        if (!ent) return;
        ptr = GEntityRefs.Get(ent).ptr;
        Assertf(ptr, "EntityRef(%s) is invalid", std::to_string(ent));
    }

    EntityRef::EntityRef(const ecs::Name &name, const Entity &ent) {
        if (!name) return;
        if (ent) {
            ptr = GEntityRefs.Set(name, ent).ptr;
        } else {
            ptr = GEntityRefs.Get(name).ptr;
        }
        Assertf(ptr, "EntityRef(%s, %s) is invalid", name.String(), std::to_string(ent));
    }

    ecs::Name EntityRef::Name() const {
        return ptr ? ptr->name : ecs::Name();
    }

    Entity EntityRef::Get(const ecs::Lock<> &lock) const {
        if (lock.GetInstance().GetInstanceId() == ecs::World.GetInstanceId()) {
            return GetLive();
        } else {
            return GetStaging();
        }
    }

    Entity EntityRef::GetLive() const {
        return ptr ? ptr->liveEntity.load() : Entity();
    }

    Entity EntityRef::GetStaging() const {
        return ptr ? ptr->stagingEntity.load() : Entity();
    }

    bool EntityRef::operator==(const EntityRef &other) const {
        if (!ptr || !other.ptr) return ptr == other.ptr;
        if (ptr == other.ptr) return true;
        auto live = ptr->liveEntity.load();
        auto staging = ptr->stagingEntity.load();
        return (live && live == other.ptr->liveEntity.load()) ||
               (staging && staging == other.ptr->stagingEntity.load());
    }

    bool EntityRef::operator==(const Entity &other) const {
        if (!ptr || !other) return false;
        return ptr->liveEntity.load() == other || ptr->stagingEntity.load() == other;
    }
} // namespace ecs
