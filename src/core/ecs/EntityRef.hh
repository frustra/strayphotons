#pragma once

#include "ecs/components/Name.hh"

#include <atomic>
#include <iostream>
#include <memory>

namespace ecs {
    class EntityRef {
    private:
        struct Ref {
            ecs::Name name;
            std::atomic<Entity> stagingEntity;
            std::atomic<Entity> liveEntity;

            Ref(const ecs::Name &name) : name(name) {}
            Ref(const Entity &ent);
        };

    public:
        EntityRef() {}
        EntityRef(const ecs::Name &name);
        EntityRef(const Entity &ent);
        EntityRef(const EntityRef &ref) : ptr(ref.ptr) {}
        EntityRef(const std::shared_ptr<Ref> &ptr) : ptr(ptr) {}

        ecs::Name Name() const {
            return ptr ? ptr->name : ecs::Name();
        }

        Entity Get() const {
            return ptr->liveEntity.load();
        }

        Entity GetStaging() const {
            return ptr->stagingEntity.load();
        }

        operator bool() const {
            return !!ptr;
        }

        bool operator!() const {
            return !ptr;
        }

        bool operator==(const EntityRef &other) const {
            return ptr == other.ptr;
        }

        bool operator!=(const EntityRef &other) const {
            return ptr != other.ptr;
        }

        void Set(const Entity &ent);

    private:
        std::shared_ptr<Ref> ptr;

        friend class EntityReferenceManager;
    };
} // namespace ecs
