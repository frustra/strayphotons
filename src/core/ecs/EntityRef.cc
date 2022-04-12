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

    EntityRef::EntityRef(const ecs::Name &name) {
        if (!name) return;
        ptr = GEntityRefs.Get(name).ptr;
    }

    EntityRef::EntityRef(const Entity &ent) {
        if (!ent) return;
        ptr = GEntityRefs.Get(ent).ptr;
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

    void EntityRef::Set(const Entity &ent) {
        Assertf(ptr, "Trying to set null EntityRef");
        Assertf(ent, "Trying to set EntityRef with null Entity");

        if (Tecs::IdentifierFromGeneration(ent.generation) == ecs::World.GetInstanceId()) {
            ptr->liveEntity = ent;
        } else {
            ptr->stagingEntity = ent;
        }
    }
} // namespace ecs
