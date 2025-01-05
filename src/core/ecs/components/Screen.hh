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
        glm::vec3 luminanceScale = glm::vec3(1);
    };

    static Component<Screen> ComponentScreen({typeid(Screen),
        "screen",
        "",
        StructField::New("target", &Screen::textureName),
        StructField::New("luminance", &Screen::luminanceScale)});
} // namespace ecs
