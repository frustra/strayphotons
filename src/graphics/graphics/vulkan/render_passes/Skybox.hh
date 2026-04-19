/*
 * Stray Photons - Copyright (C) 2026 Jacob Wirth
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "Common.hh"
#include "strayphotons/cpp/Async.hh"

namespace ecs {
    struct View;
}

namespace sp::vulkan::renderer {
    class Skybox {
    public:
        void AddPass(RenderGraph &graph);

        bool PreloadTextures(DeviceContext &device);

    private:
        AsyncPtr<ImageView> randomTex;
    };
} // namespace sp::vulkan::renderer
