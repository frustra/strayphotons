#pragma once

#include <ecs/Components.hh>

namespace ecs {
    enum class OwnerType { INVALID = 0, GAME_LOGIC, XR_MANAGER };

    struct Owner {
        Owner() : id(0), type(OwnerType::INVALID) {}
        Owner(OwnerType type) : id(0), type(type) {}

        inline bool operator==(const Owner &other) const {
            return type == other.type;
        }

        size_t id;
        OwnerType type;
    };

    static Component<Owner> ComponentCreator("owner");
} // namespace ecs
