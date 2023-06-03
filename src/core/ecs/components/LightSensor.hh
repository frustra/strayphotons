#pragma once

#include <ecs/Components.hh>
#include <ecs/Ecs.hh>
#include <glm/glm.hpp>

namespace ecs {
    class LightSensor {
    public:
        // Required parameters.
        glm::vec3 position = {0, 0, 0}; // In model space.
        glm::vec3 direction = {0, 0, -1}; // In model space.

        // Updated automatically.
        glm::vec3 illuminance;

        LightSensor() {}
        LightSensor(glm::vec3 p, glm::vec3 n) : position(p), direction(n) {}
    };

    static StructMetadata MetadataLightSensor(typeid(LightSensor),
        "light_sensor",
        StructField::New("position", &LightSensor::position),
        StructField::New("direction", &LightSensor::direction),
        StructField::New("color_value", &LightSensor::illuminance));
    static Component<LightSensor> ComponentLightSensor(MetadataLightSensor);
} // namespace ecs
