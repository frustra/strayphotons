#include "EntityRef.hh"

#include "ecs/EcsImpl.hh"
#include "ecs/ReferenceManager.hh"

#include <atomic>

namespace ecs {
    EntityRef::Ref::Ref(const Entity &ent) {
        if (!ent) return;

        if (IsLive(ent)) {
            liveEntity = ent;
        } else if (IsStaging(ent)) {
            stagingEntity = ent;
        } else {
            Abortf("Invalid EntityRef entity: %s", std::to_string(ent));
        }
    }

    EntityRef::EntityRef(const Entity &ent) {
        if (!ent) return;
        ptr = GetRefManager().GetEntity(ent).ptr;
    }

    EntityRef::EntityRef(const ecs::Name &name, const Entity &ent) {
        if (!name) return;
        if (ent) {
            ptr = GetRefManager().SetEntity(name, ent).ptr;
        } else {
            ptr = GetRefManager().GetEntity(name).ptr;
        }
        Assertf(ptr, "EntityRef(%s, %s) is invalid", name.String(), std::to_string(ent));
    }

    ecs::Name EntityRef::Name() const {
        return ptr ? ptr->name : ecs::Name();
    }

    Entity EntityRef::Get(const ecs::Lock<> &lock) const {
        if (IsLive(lock)) {
            return GetLive();
        } else if (IsStaging(lock)) {
            return GetStaging();
        } else {
            Abortf("Invalid EntityRef lock: %u", lock.GetInstance().GetInstanceId());
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

    bool EntityRef::operator<(const EntityRef &other) const {
        return Name() < other.Name();
    }
} // namespace ecs
