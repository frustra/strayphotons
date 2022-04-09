#pragma once

#include <functional>
#include <iostream>
#include <string_view>

namespace sp {
    class Scene;
}

namespace ecs {
    struct Name {
        std::string scene, entity;

        Name() {}
        Name(std::string_view scene, std::string_view entity);

        bool Parse(const std::string &fullName, const Name &scope = Name());

        std::string String() const {
            if (scene.empty()) return entity;
            return scene + ":" + entity;
        }

        operator bool() const {
            return !entity.empty();
        }

        bool operator==(const Name &other) const {
            return scene == other.scene && entity == other.entity;
        }
    };

    std::ostream &operator<<(std::ostream &out, const Name &v);
} // namespace ecs

namespace std {
    template<>
    struct hash<ecs::Name> {
        std::size_t operator()(const ecs::Name &n) const;
    };
} // namespace std
