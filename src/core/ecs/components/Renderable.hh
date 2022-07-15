#pragma once

#include "assets/Async.hh"
#include "core/Common.hh"
#include "core/EnumTypes.hh"
#include "ecs/Components.hh"
#include "ecs/EntityRef.hh"

#include <glm/glm.hpp>

namespace sp {
    class Gltf;
}

template<>
struct magic_enum::customize::enum_range<ecs::VisibilityMask> {
    static constexpr bool is_flags = true;
};

namespace ecs {
    enum class VisibilityMask {
        None = 0,
        DirectCamera = 1 << 0,
        DirectEye = 1 << 1,
        Reflected = 1 << 2,
        LightingShadow = 1 << 3,
        LightingVoxel = 1 << 4,
        Optics = 1 << 5,
        OutlineSelection = 1 << 6,
    };

    struct Renderable {
        Renderable() : meshIndex(0) {}
        Renderable(sp::AsyncPtr<sp::Gltf> model, size_t meshIndex = 0) : model(model), meshIndex(meshIndex) {}

        sp::AsyncPtr<sp::Gltf> model;
        size_t meshIndex = 0;

        struct Joint {
            EntityRef entity;
            glm::mat4 inverseBindPose;
        };
        vector<Joint> joints; // list of entities corresponding to the "joints" array of the skin

        VisibilityMask visibility = ~VisibilityMask::OutlineSelection;
        glm::vec3 emissive = {0.0f, 0.0f, 0.0f};
    };

    static Component<Renderable> ComponentRenderable("renderable");

    template<>
    bool Component<Renderable>::Load(const EntityScope &scope, Renderable &dst, const picojson::value &src);
} // namespace ecs
