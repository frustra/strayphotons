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
            VISIBLE_OPTICS,
            VISIBLE_OUTLINE_SELECTION,
            VISIBILITY_COUNT,
        };

        using VisibilityMask = std::bitset<VISIBILITY_COUNT>;

        Renderable() : meshIndex(0) {}
        Renderable(sp::AsyncPtr<sp::Gltf> model, size_t meshIndex = 0) : model(model), meshIndex(meshIndex) {}

        sp::AsyncPtr<sp::Gltf> model;
        size_t meshIndex;

        struct Joint {
            Name entity; // TODO use EntityRef
            glm::mat4 inverseBindPose;
        };
        vector<Joint> joints; // list of entities corresponding to the "joints" array of the skin

        VisibilityMask visibility = VisibilityMask().set().reset(Visibility::VISIBLE_OUTLINE_SELECTION);
        glm::vec3 emissive = {0.0f, 0.0f, 0.0f};
    };

    static Component<Renderable> ComponentRenderable("renderable");

    template<>
    bool Component<Renderable>::Load(ScenePtr scenePtr, const Name &scope, Renderable &dst, const picojson::value &src);
} // namespace ecs
