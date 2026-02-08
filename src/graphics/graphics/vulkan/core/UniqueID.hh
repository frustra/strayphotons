/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "common/Common.hh"

namespace sp::vulkan {
    class DeviceContext;

    typedef uint64_t UniqueID;

    class HasUniqueID {
    public:
        HasUniqueID();

        UniqueID GetUniqueID() const {
            return uniqueID;
        }

    private:
        const UniqueID uniqueID;
    };
} // namespace sp::vulkan
