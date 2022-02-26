#pragma once

#include "ecs/Ecs.hh"
#include "ecs/components/Name.hh"

#include <string>

namespace ecs {
    class NamedEntity {
    public:
        NamedEntity() {}
        NamedEntity(const std::string &sceneName, const std::string &entityName, Entity ent = Entity())
            : name(sceneName, entityName), ent(ent) {}
        NamedEntity(const ecs::Name &name, Entity ent = Entity()) : name(name), ent(ent) {}

        const ecs::Name &Name() const {
            return name;
        }

        const Entity &Get(Lock<Read<ecs::Name>> lock);

        Entity Get(Lock<Read<ecs::Name>> lock) const;

        bool operator==(const NamedEntity &other) const {
            return name && name == other.name;
        }

        bool operator!=(const NamedEntity &other) const {
            return !(*this == other);
        }

        bool operator==(const ecs::Name &other) const {
            return name && name == other;
        }

        bool operator!=(const ecs::Name &other) const {
            return !(*this == other);
        }

    private:
        ecs::Name name;
        Entity ent;
    };
} // namespace ecs
