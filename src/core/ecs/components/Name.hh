#pragma once

#include <functional>
#include <string>

namespace sp {
    class Scene;
}

namespace ecs {
    struct Name {
        std::string scene, entity;

        Name() {}
        Name(std::string scene, std::string entity) : scene(scene), entity(entity) {}

        bool Parse(std::string fullName, const sp::Scene *currentScene = GLM_NULLPTR);

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
} // namespace ecs

namespace std {
    template<>
    struct hash<ecs::Name> {
        std::size_t operator()(const ecs::Name &n) const;
    };
} // namespace std
