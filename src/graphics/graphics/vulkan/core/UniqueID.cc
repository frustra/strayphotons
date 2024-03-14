/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "UniqueID.hh"

#include "graphics/vulkan/core/DeviceContext.hh"

namespace sp::vulkan {
    static UniqueID NextUniqueID() {
        static UniqueID lastUniqueID = 0;
        return ++lastUniqueID;
    }

    HasUniqueID::HasUniqueID() : uniqueID(NextUniqueID()) {}
} // namespace sp::vulkan
