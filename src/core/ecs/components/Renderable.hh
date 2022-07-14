#pragma once

#include "assets/Async.hh"
#include "core/EnumTypes.hh"
#include "ecs/Components.hh"
#include "ecs/EntityRef.hh"

#include <glm/glm.hpp>

namespace sp {
    class Gltf;
}

namespace ecs {
    struct Renderable {
        enum class Visibility {
            DirectCamera = 0,
            DirectEye,
            Reflected,
            LightingShadow,
            LightingVoxel,
            Optics,
            OutlineSelection,
            Count,
        };

        using VisibilityMask = sp::EnumFlags<Visibility>;

        Renderable() : meshIndex(0) {}
        Renderable(sp::AsyncPtr<sp::Gltf> model, size_t meshIndex = 0) : model(model), meshIndex(meshIndex) {}

        sp::AsyncPtr<sp::Gltf> model;
        size_t meshIndex;

        struct Joint {
            EntityRef entity;
            glm::mat4 inverseBindPose;
        };
        vector<Joint> joints; // list of entities corresponding to the "joints" array of the skin

        VisibilityMask visibility = VisibilityMask().set().reset(Visibility::OutlineSelection);
        glm::vec3 emissive = {0.0f, 0.0f, 0.0f};
    };

    static Component<Renderable> ComponentRenderable("renderable");

    template<>
    bool Component<Renderable>::Load(const EntityScope &scope, Renderable &dst, const picojson::value &src);
} // namespace ecs
