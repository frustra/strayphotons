/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "Common.hh"
#include "graphics/vulkan/scene/GPUScene.hh"

#include <optional>
#include <string>

namespace sp::vulkan::renderer {
    void AddLightSensors(RenderGraph &graph,
        GPUScene &scene,
        ecs::Lock<ecs::Read<ecs::LightSensor, ecs::TransformSnapshot>> lock);
} // namespace sp::vulkan::renderer
