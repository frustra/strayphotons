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
        "",
        StructField::New("intensity",
            "The brightness of the light measured in candela (lumens per solid angle). This value is ignored if "
            "**illuminance** != 0.",
            &Light::intensity),
        StructField::New("illuminance",
            "The brightness of the light measured in lux (lumens per square meter). This has the behavior of making "
            "the light's brightness independant of distance from the light. Overrides **intensity** field.",
            &Light::illuminance),
        StructField::New("spot_angle",
            "The angle from the middle to the edge of the light's field of view cone. "
            "This will be half the light's overall field of view.",
            &Light::spotAngle),
        StructField::New("tint", "The color of light to be emitted", &Light::tint),
        StructField::New("gel",
            "A lighting gel (or light filter) texture to be applied to this light. "
            "Asset textures can be referenced with the format \"asset:<asset_path>.png\", "
            "or render graph outputs can be referenced with the format \"graph:<graph_output_name>\"",
            &Light::gelName),
        StructField::New("on", "A flag to turn this light on and off without changing the light's values.", &Light::on),
        StructField::New("shadow_map_size",
            "All shadow maps are square powers of 2 in resolution. Each light's shadow map resolution is defined as "
            "`2^shadow_map_size`. For example, a map size of 10 would result in a 1024x1024 shadow map resolution.",
            &Light::shadowMapSize),
        StructField::New("shadow_map_clip",
            "The near and far clipping plane distances for this light. For example, with a clip value of `[1, 10]`, "
            "light won't start hitting objects until the near plane, 1 meter from the light. "
            "The light will then cast shadows for the next 9 meters until the far plane, 10 meters from the light.",
            &Light::shadowMapClip));
    static Component<Light> ComponentLight(MetadataLight);
} // namespace ecs
