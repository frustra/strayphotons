/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

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
        glm::vec3 illuminance = glm::vec3(0);

        LightSensor() {}
        LightSensor(glm::vec3 p, glm::vec3 n) : position(p), direction(n) {}
    };

    static Component<LightSensor> ComponentLightSensor({typeid(LightSensor),
        "light_sensor",
        "",
        StructField::New("position", &LightSensor::position),
        StructField::New("direction", &LightSensor::direction),
        StructField::New("color_value", &LightSensor::illuminance)});
} // namespace ecs
