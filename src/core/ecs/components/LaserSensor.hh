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

        // updated automatically
        glm::vec3 illuminance;
    };

    static StructMetadata MetadataLaserSensor(typeid(LaserSensor),
        "laser_sensor",
        R"(
A laser sensor turns this entity's [`physics`](#physics-component) shapes into a receiver for laser signals.  
Each physics frame [`laser_emitter`](#laser_emitter-component) entities will have their paths updated, 
and any `laser_sensor` entities hit by lasers will output 3 color signals and a `value` signal based on the sensor's threshold.

The following signals are written to this entity's [`signal_output` Component](General_Components.md#signal_output-component):
```json
// Total incoming laser light by color:
light_value_r
light_value_g
light_value_b

// Threshold value outputs 0.0 or 1.0:
value
```
)",
        StructField::New("threshold",
            "The `value` signal is set to **true** when all input RGB values are above their corresponding threshold. "
            "This is equivelent to the signal expression: `light_value_r >= threshold.x && "
            "light_value_g >= threshold.y && light_value_b >= threshold.z`",
            &LaserSensor::threshold));
    static Component<LaserSensor> ComponentLaserSensor(MetadataLaserSensor);
} // namespace ecs
