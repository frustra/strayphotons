#pragma once

#include "ecs/components/Name.hh"

#include <memory>

namespace ecs {
    class EntityRef {
    private:
        struct Ref;

    public:
        EntityRef() {}
        EntityRef(const ecs::Name &name);
        EntityRef(const Entity &ent);
        EntityRef(const EntityRef &ref) : ptr(ref.ptr) {}
        EntityRef(const std::shared_ptr<Ref> &ptr) : ptr(ptr) {}

        ecs::Name Name() const;
        Entity Get() const;
        Entity GetStaging() const;

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
