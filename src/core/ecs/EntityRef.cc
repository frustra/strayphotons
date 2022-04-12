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
    }

    EntityRef::EntityRef(const ecs::Name &name, const Entity &ent) {
        if (!name) return;
        ptr = GEntityRefs.Get(name).ptr;
        if (ent) Set(ent);
    }

    ecs::Name EntityRef::Name() const {
        return ptr ? ptr->name : ecs::Name();
    }

    Entity EntityRef::Get() const {
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
