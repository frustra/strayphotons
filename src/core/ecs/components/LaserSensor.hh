#pragma once

#include "ecs/Components.hh"
#include "ecs/Ecs.hh"

#include <glm/glm.hpp>

namespace ecs {
    struct LaserSensor {
        glm::vec3 threshold;

        // updated automatically
        glm::vec3 illuminance;
        bool triggered = false;
    };

    static Component<LaserSensor> ComponentLaserSensor("laser_sensor",
        ComponentField::New("threshold", &LaserSensor::threshold));
} // namespace ecs
