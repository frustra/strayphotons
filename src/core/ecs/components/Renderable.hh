#pragma once

#include "assets/Async.hh"
#include "ecs/Components.hh"

#include <glm/glm.hpp>

namespace sp {
    class Gltf;
}

namespace ecs {
    struct Renderable {
        enum Visibility {
            VISIBLE_DIRECT_CAMERA = 0,
            VISIBLE_DIRECT_EYE,
            VISIBLE_REFLECTED,
            VISIBLE_LIGHTING_SHADOW,
            VISIBLE_LIGHTING_VOXEL,
            VISIBILITY_COUNT,
        };

        using VisibilityMask = std::bitset<VISIBILITY_COUNT>;

        Renderable() {}
        Renderable(sp::AsyncPtr<sp::Gltf> model) : model(model) {}

        sp::AsyncPtr<sp::Gltf> model;
        VisibilityMask visibility = VisibilityMask().set();
        glm::vec3 emissive = {0.0f, 0.0f, 0.0f};
    };

    static Component<Renderable> ComponentRenderable("renderable");

    template<>
    bool Component<Renderable>::Load(sp::Scene *scene, Renderable &dst, const picojson::value &src);
} // namespace ecs
