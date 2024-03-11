/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "VkCommon.hh"

#include "common/Common.hh"
#include "common/Logging.hh"

namespace sp::vulkan {
    void AssertVKSuccess(vk::Result result, std::string message) {
        if (result == vk::Result::eSuccess) return;
        Abortf("%s (%s)", message, vk::to_string(result));
    }

    void AssertVKSuccess(VkResult result, std::string message) {
        AssertVKSuccess(static_cast<vk::Result>(result), message);
    }
} // namespace sp::vulkan
