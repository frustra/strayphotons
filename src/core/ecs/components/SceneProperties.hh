#pragma once

#include "ecs/Ecs.hh"
#include "ecs/StructMetadata.hh"
#include "ecs/components/Transform.h"

#include <functional>
#include <glm/glm.hpp>

namespace ecs {
    struct SceneProperties {
        Transform rootTransform;
        Transform gravityTransform = Transform();
        glm::vec3 fixedGravity = glm::vec3(0, -9.81, 0);
        std::function<glm::vec3(glm::vec3)> gravityFunction;

        static const SceneProperties &Get(Lock<Read<SceneProperties>> lock, Entity ent);

        glm::vec3 GetGravity(glm::vec3 worldPosition) const;

        bool operator==(const SceneProperties &other) const;
    };

    static StructMetadata MetadataSceneProperties(typeid(SceneProperties),
        "SceneProperties",
        StructField::New("root_transform", &SceneProperties::rootTransform, FieldAction::AutoApply),
        StructField::New("gravity_transform", &SceneProperties::gravityTransform),
        StructField::New("gravity", &SceneProperties::fixedGravity));
    static Component<SceneProperties> ComponentSceneProperties(MetadataSceneProperties, "scene_properties");

    template<>
    bool StructMetadata::Load<SceneProperties>(SceneProperties &dst, const picojson::value &src);
    template<>
    void StructMetadata::Save<SceneProperties>(const EntityScope &scope,
        picojson::value &dst,
        const SceneProperties &src,
        const SceneProperties *def);

    template<>
    void Component<SceneProperties>::Apply(SceneProperties &dst, const SceneProperties &src, bool liveTarget);
} // namespace ecs
