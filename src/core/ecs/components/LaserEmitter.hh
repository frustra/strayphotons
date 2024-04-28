/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "common/Common.hh"
#include "ecs/Components.hh"
#include "ecs/Ecs.hh"

#include <glm/glm.hpp>

namespace ecs {
    struct LaserEmitter {
        float intensity = 1.0f; // multiplier applied to color to produce the final luminance
        sp::color_t color = glm::vec3(0); // HDR value, added to laser_color_* signal
        bool on = true;
        float startDistance = 0.0f;
    };

    static StructMetadata MetadataLaserEmitter(typeid(LaserEmitter),
        "laser_emitter",
        "",
        StructField::New("intensity", &LaserEmitter::intensity),
        StructField::New("color", &LaserEmitter::color),
        StructField::New("on", &LaserEmitter::on),
        StructField::New("start_distance", &LaserEmitter::startDistance));
    static Component<LaserEmitter> ComponentLaserEmitter(MetadataLaserEmitter);
} // namespace ecs
