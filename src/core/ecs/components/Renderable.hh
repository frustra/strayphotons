/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

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

namespace ecs {
    enum class VisibilityMask;
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
        Transparent = 1 << 2,
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
        sp::color_alpha_t colorOverride = glm::vec4(-1);
        glm::vec2 metallicRoughnessOverride = glm::vec2(-1);

        bool IsVisible(VisibilityMask viewMask) const {
            return (visibility & viewMask) == viewMask;
        }
    };

    static StructMetadata MetadataRenderable(typeid(Renderable),
        "renderable",
        R"(
Models are loaded from the `assets/models/` folder. `.glb` and `.gltf` are supported,
and models can be loaded from either `assets/models/<model_name>.gltf` or `assets/models/<model_name>/model_name.gltf`.

Note for GLTF models with multiple meshes:  
It is usually preferred to load the model using the [gltf Prefab Script](Prefab_Scripts.md#gltf-prefab) to automatically generate the correct transform tree and entity structure.
)",
        StructField::New("model",
            "Name of the GLTF model to display. Models are loaded from the `assets/models/` folder.",
            &Renderable::modelName),
        StructField::New("mesh_index",
            "The index of the mesh to render from the GLTF model. "
            "Note, multi-mesh GLTF models can be automatically expanded into entities using the `gltf` prefab.",
            &Renderable::meshIndex),
        StructField::New("visibility", "Visibility mask for different render passes.", &Renderable::visibility),
        StructField::New("emissive",
            "Emissive multiplier to turn this model into a light source",
            &Renderable::emissiveScale),
        StructField::New("color_override",
            "Override the mesh's texture to a flat RGBA color. "
            "Values are in the range 0.0 to 1.0. -1 means the original color is used.",
            &Renderable::colorOverride),
        StructField::New("metallic_roughness_override",
            "Override the mesh's metallic and roughness material properties. "
            "Values are in the range 0.0 to 1.0. -1 means the original material is used.",
            &Renderable::metallicRoughnessOverride));
    static Component<Renderable> ComponentRenderable(MetadataRenderable);

    template<>
    bool StructMetadata::Load<Renderable>(Renderable &dst, const picojson::value &src);
    template<>
    void Component<Renderable>::Apply(Renderable &dst, const Renderable &src, bool liveTarget);
} // namespace ecs
