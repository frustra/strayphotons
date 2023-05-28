#pragma once

#include "core/Common.hh"
#include "ecs/Components.hh"
#include "ecs/Ecs.hh"

#include <glm/glm.hpp>

namespace ecs {
    struct Light {
        float intensity = 0;
        float illuminance = 0;
        sp::angle_t spotAngle = 0.0f;
        sp::color_t tint = glm::vec3(1);
        string gelName;
        bool on = true;
        uint32_t shadowMapSize = 9; // shadow map will have a width and height of 2^shadowMapSize
        glm::vec2 shadowMapClip = {0.1, 256}; // near and far plane
    };

    static StructMetadata MetadataLight(typeid(Light),
        "light",
        StructField::New("intensity", &Light::intensity),
        StructField::New("illuminance", &Light::illuminance),
        StructField::New("spotAngle", &Light::spotAngle),
        StructField::New("tint", &Light::tint),
        StructField::New("gel", &Light::gelName),
        StructField::New("on", &Light::on),
        StructField::New("shadowMapSize", &Light::shadowMapSize),
        StructField::New("shadowMapClip", &Light::shadowMapClip));
    static Component<Light> ComponentLight(MetadataLight);
} // namespace ecs
