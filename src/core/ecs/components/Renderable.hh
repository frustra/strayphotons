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
        Transparency = 1 << 2,
        LightingShadow = 1 << 3,
        LightingVoxel = 1 << 4,
        Optics = 1 << 5,
        OutlineSelection = 1 << 6,
    };

    struct Renderable {
        Renderable() {}
        Renderable(const std::string &modelName, size_t meshIndex = 0);
        Renderable(const std::string &modelName, sp::AsyncPtr<sp::Gltf> model, size_t meshIndex = 0)
            : modelName(modelName), model(model), meshIndex(meshIndex) {}

        std::string modelName;
        sp::AsyncPtr<sp::Gltf> model;
        size_t meshIndex = 0;

        struct Joint {
            EntityRef entity;
            glm::mat4 inverseBindPose;
        };
        vector<Joint> joints; // list of entities corresponding to the "joints" array of the skin

        VisibilityMask visibility = VisibilityMask::DirectCamera | VisibilityMask::DirectEye |
                                    VisibilityMask::LightingShadow | VisibilityMask::LightingVoxel;
        float emissiveScale = 0;

        bool IsVisible(VisibilityMask viewMask) const {
            return (visibility & viewMask) == viewMask;
        }
    };

    static Component<Renderable> ComponentRenderable("renderable",
        ComponentField::New("model", &Renderable::modelName),
        ComponentField::New("mesh_index", &Renderable::meshIndex),
        ComponentField::New("visibility", &Renderable::visibility),
        ComponentField::New("emissive", &Renderable::emissiveScale));

    template<>
    bool Component<Renderable>::Load(const EntityScope &scope, Renderable &dst, const picojson::value &src);
    template<>
    void Component<Renderable>::Apply(const Renderable &src, Lock<AddRemove> lock, Entity dst);
} // namespace ecs
