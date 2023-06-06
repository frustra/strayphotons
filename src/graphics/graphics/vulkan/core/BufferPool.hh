/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "graphics/vulkan/core/Memory.hh"
#include "graphics/vulkan/core/VkCommon.hh"

#include <robin_hood.h>

namespace sp::vulkan {
    class BufferPool {
    public:
        BufferPool(DeviceContext &device) : device(device) {}
        BufferPtr Get(const BufferDesc &desc);
        void Tick();
        void LogStats() const;

    private:
        DeviceContext &device;

        struct BufferEntry {
            BufferPtr ptr;
            int unusedFrames;
        };

        struct BufferList {
            vector<BufferEntry> free;
            vector<BufferEntry> pendingFree;
            vector<BufferEntry> pending;
        };
        robin_hood::unordered_map<BufferDesc, BufferList> buffers;
    };
} // namespace sp::vulkan
