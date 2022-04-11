#include "EntityRef.hh"

#include "ecs/EcsImpl.hh"
#include "ecs/EntityReferenceManager.hh"

#include <atomic>

namespace ecs {
    EntityRef::Ref::Ref(const Entity &ent) {
        if (!ent) return;

        if (Tecs::IdentifierFromGeneration(ent.generation) == ecs::World.GetInstanceId()) {
            // This is a live ECS entity
            liveEntity = ent;
        } else {
            // This is (likely) a staging ECS entity
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

    Entity EntityRef::Get() const {
        return ptr->liveEntity.load();
    }

    Entity EntityRef::GetStaging() const {
        return ptr->stagingEntity.load();
    }

    void EntityRef::Set(const Entity &ent) {
        Assertf(ptr, "Trying to set null EntityRef");
        Assertf(ent, "Trying to set EntityRef with null Entity");

        if (Tecs::IdentifierFromGeneration(ent.generation) == ecs::World.GetInstanceId()) {
            // This is a live ECS entity
            ptr->liveEntity = ent;
        } else {
            // This is (likely) a staging ECS entity
            ptr->stagingEntity = ent;
        }
    }
} // namespace ecs
