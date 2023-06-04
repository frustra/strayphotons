#pragma once

#include <functional>
#include <iostream>
#include <string>
#include <string_view>

namespace sp {
    class Scene;
}

namespace ecs {
    struct Name;
    using EntityScope = Name;

    static const char *DocsDescriptionName = R"(
This component is required on all entities to allow for name-based references.
If no name is provided upon entity creation, an auto-generated name will be filled in.

Names are in the form:
> *<scene_name>*:*<entity_name>*

An example could be `hello_world:platform`.

By leaving out the scene qualifier, names can also be defined relative to their entity scope.
Inside the scene definiton the entity scope might be "hello_world:",
meaning both `hello_world:platform` and `platform` would reference the same entity.

Relative names specified in a template take the form:
> *<scene_name>*:*<root_name>*.*<relative_name>*

The special `scoperoot` alias can also be used inside a template to reference the parent entity.
)";

    struct Name {
        std::string scene, entity;

        Name() {}
        Name(const std::string_view &scene, const std::string_view &entity);
        Name(const std::string_view &relativeName, const EntityScope &scope);
        Name(const Name &other, const EntityScope &scope);

        bool Parse(const std::string_view &relativeName, const EntityScope &scope);

        std::string String() const {
            if (scene.empty()) return entity;
            return scene + ":" + entity;
        }

        explicit operator bool() const {
            return !entity.empty();
        }

        bool operator==(const Name &) const = default;
        bool operator<(const Name &other) const {
            return scene == other.scene ? entity < other.entity : scene < other.scene;
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
