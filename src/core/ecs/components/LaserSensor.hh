/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "ecs/Components.hh"
#include "ecs/Ecs.hh"

#include <glm/glm.hpp>

namespace ecs {
    struct LaserSensor {
        glm::vec3 threshold = glm::vec3(0.5f, 0.5f, 0.5f);

        // Updated based on light path by the Physics thread
        std::array<SignalExpression, 3> illuminance = {};
    };

    static Component<LaserSensor> ComponentLaserSensor(
        {typeid(LaserSensor), "laser_sensor", "", StructField::New("threshold", &LaserSensor::threshold)});
} // namespace ecs
