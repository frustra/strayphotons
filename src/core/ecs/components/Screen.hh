/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "ecs/Components.hh"

namespace ecs {
    struct Screen {
        string textureName;
        glm::ivec2 resolution = glm::ivec2(1000, 1000); // 1000 pixels per meter world-scale (~25 dpi)
        glm::vec2 scale = glm::vec2(1.0f);
        glm::vec3 luminanceScale = glm::vec3(1);
    };

    static EntityComponent<Screen> ComponentScreen("screen",
        "",
        StructField::New("texture", &Screen::textureName),
        StructField::New("resolution", &Screen::resolution),
        StructField::New("scale", &Screen::scale),
        StructField::New("luminance", &Screen::luminanceScale));
} // namespace ecs
