#pragma once

#include "ecs/Ecs.hh"
#include "ecs/components/Name.hh"

#include <string>

namespace ecs {
    class NamedEntity {
    public:
        NamedEntity() {}
        NamedEntity(const Tecs::Entity &ent) : ent(ent) {}
        NamedEntity(const ecs::Name &name) : name(name) {}
        NamedEntity(const std::string &sceneName, const std::string &entityName) : name(sceneName, entityName) {}

        const ecs::Name &Name() const {
            return name;
        }

        operator const Entity &() const {
            return ent;
        }

        const Entity &Get(Lock<Read<ecs::Name>> lock);

        bool operator==(const NamedEntity &other) const {
            return (name && name == other.name) || (ent && ent == other.ent);
        }

        bool operator!=(const NamedEntity &other) const {
            return !(*this == other);
        }

    private:
        ecs::Name name;
        Entity ent;
    };
} // namespace ecs
