/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "game/SceneRef.hh"

namespace ecs {
    struct ActiveScene {
        sp::SceneRef scene;

        ActiveScene() {}
        ActiveScene(const sp::SceneRef &scene) : scene(scene) {}
    };
} // namespace ecs

TECS_GLOBAL_COMPONENT(ecs::ActiveScene);
