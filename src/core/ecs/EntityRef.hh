#pragma once

#include "ecs/components/Name.hh"

#include <atomic>
#include <iostream>
#include <memory>

namespace ecs {
    struct Name;
    class EntityReferenceManager;

    class EntityRef {
    private:
        struct Ref {
            ecs::Name name;
            std::atomic<Entity> entity;

            Ref(const ecs::Name &name, const Entity &ent) : name(name), entity(ent) {}
        };

    public:
        EntityRef() {}
        EntityRef(const Entity &ent) : ptr(make_shared<Ref>(ecs::Name(), ent)) {}

        ecs::Name Name() const {
            return ptr ? ptr->name : ecs::Name();
        }

        Entity Get() const {
            Assertf(ptr, "EntityRef is null");
            return ptr->entity.load();
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

    private:
        EntityRef(const ecs::Name &name, const Entity &ent) : ptr(make_shared<Ref>(name, ent)) {}

        void Set(const Entity &ent) {
            Assertf(ptr, "Can't set entity on null EntityRef");
            ptr->entity.store(ent);
        }

        std::shared_ptr<Ref> ptr;

        friend class EntityReferenceManager;
    };
} // namespace ecs
