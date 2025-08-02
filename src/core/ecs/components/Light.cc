/*
 * Stray Photons - Copyright (C) 2025 Jacob Wirth
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "Light.hh"

#include "common/Logging.hh"
#include "ecs/EcsImpl.hh"

#include <picojson/picojson.h>

namespace ecs {
    template<>
    void EntityComponent<Light>::Apply(Light &dst, const Light &src, bool liveTarget) {
        if (liveTarget || (dst.filterName.empty() && !src.filterName.empty())) {
            dst.filterName = src.filterName;
        }
    }
} // namespace ecs
