#pragma once

#include "ecs/Components.hh"
#include "ecs/Ecs.hh"

#include <glm/glm.hpp>

namespace ecs {
    struct LaserSensor {
        glm::vec3 threshold = glm::vec3(0.5f, 0.5f, 0.5f);

        // updated automatically
        glm::vec3 illuminance;
    };

    static StructMetadata MetadataLaserSensor(typeid(LaserSensor),
        "laser_sensor",
        "",
        StructField::New("threshold", &LaserSensor::threshold));
    static Component<LaserSensor> ComponentLaserSensor(MetadataLaserSensor);
} // namespace ecs
