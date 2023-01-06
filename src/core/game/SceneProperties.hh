#pragma once

#include "ecs/StructMetadata.hh"
#include "ecs/components/Transform.h"

#include <functional>
#include <glm/glm.hpp>

namespace sp {
    struct SceneProperties {
        ecs::Transform gravityTransform = ecs::Transform();
        glm::vec3 fixedGravity = glm::vec3(0, -9.81, 0);
        std::function<glm::vec3(glm::vec3)> gravityFunction;

        glm::vec3 GetGravity(glm::vec3 worldPosition) const {
            if (gravityFunction) {
                auto gravityPos = gravityTransform.GetInverse() * glm::vec4(worldPosition, 1);
                return fixedGravity + gravityTransform.GetRotation() * gravityFunction(gravityPos);
            }
            return fixedGravity;
        }

        bool operator==(const SceneProperties &other) const;
    };
} // namespace sp

namespace ecs {
    using namespace sp;

    static StructMetadata MetadataSceneProperties(typeid(SceneProperties),
        StructField::New("gravity_transform", &SceneProperties::gravityTransform),
        StructField::New("gravity", &SceneProperties::fixedGravity));
    template<>
    bool StructMetadata::Load<SceneProperties>(const EntityScope &scope,
        SceneProperties &dst,
        const picojson::value &src);
    template<>
    void StructMetadata::Save<SceneProperties>(const EntityScope &scope,
        picojson::value &dst,
        const SceneProperties &src);
} // namespace ecs
