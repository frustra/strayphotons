/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "Common.hh"
#include "console/CFunc.hh"
#include "strayphotons/LockFreeMutex.hh"

#include <string>
#include <vector>

namespace sp::vulkan::renderer {
    class Screenshots {
    public:
        Screenshots();
        void AddPass(RenderGraph &graph);

    private:
        CFuncCollection funcs;
        LockFreeMutex screenshotMutex;
        struct PendingScreenshot {
            std::string path, resource;
            bool assert = false;
        };
        std::vector<PendingScreenshot> pendingScreenshots;
    };

    void WriteScreenshot(DeviceContext &device, const std::string &path, const ImageViewPtr &view);
} // namespace sp::vulkan::renderer
