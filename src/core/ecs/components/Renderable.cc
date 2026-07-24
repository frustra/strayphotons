/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justine Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "Renderable.hh"

#include "assets/AssetManager.hh"

#include <cstdint>
#include <picojson.h>

namespace ecs {
    template<>
    void EntityComponent<Renderable>::Apply(Renderable &dst, const Renderable &src, bool liveTarget) {
        if (liveTarget || (dst.modelName.empty() && !src.modelName.empty())) {
            dst.modelName = src.modelName;
        }
        if (dst.joints.empty()) dst.joints = src.joints;
    }

    Renderable::Renderable(std::string_view modelName, uint32_t meshIndex)
        : modelName(modelName), meshIndex(meshIndex) {
        if (modelName.empty()) {
            visibility = VisibilityMask::None;
        }
    }
} // namespace ecs
