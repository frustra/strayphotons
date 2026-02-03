/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "Common.hh"
#include "common/Async.hh"

namespace sp::vulkan::renderer {
    class SMAA {
    public:
        void AddPass(RenderGraph &graph);

        bool PreloadTextures(DeviceContext &device);

    private:
        AsyncPtr<ImageView> areaTex, searchTex;
    };
} // namespace sp::vulkan::renderer
