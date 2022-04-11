#pragma once

#ifndef SP_WASM_BUILD
    #include <functional>
    #include <iostream>
#endif

namespace sp {
    class Scene;
}

namespace ecs {
    struct Name {
        std::string scene, entity;

        Name() {}
        Name(const std::string &scene, const std::string &entity);

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

#ifndef SP_WASM_BUILD
    std::ostream &operator<<(std::ostream &out, const Name &v);
#endif
} // namespace ecs

#ifndef SP_WASM_BUILD
namespace std {
    template<>
    struct hash<ecs::Name> {
        std::size_t operator()(const ecs::Name &n) const;
    };
} // namespace std
#endif
