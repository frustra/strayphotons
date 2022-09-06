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

    static Component<Light> ComponentLight("light",
        ComponentField::New("intensity", &Light::intensity),
        ComponentField::New("illuminance", &Light::illuminance),
        ComponentField::New("spotAngle", &Light::spotAngle),
        ComponentField::New("tint", &Light::tint),
        ComponentField::New("gel", &Light::gelName),
        ComponentField::New("on", &Light::on),
        ComponentField::New("shadowMapSize", &Light::shadowMapSize),
        ComponentField::New("shadowMapClip", &Light::shadowMapClip));
} // namespace ecs
