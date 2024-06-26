/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "Renderable.hh"

#include "assets/AssetManager.hh"
#include "common/Logging.hh"
#include "ecs/EcsImpl.hh"

#include <picojson/picojson.h>

namespace ecs {
    template<>
    bool StructMetadata::Load<Renderable>(Renderable &renderable, const picojson::value &src) {
        if (!renderable.modelName.empty()) {
            renderable.model = sp::Assets().LoadGltf(renderable.modelName);
        }
        return true;
    }

    template<>
    void Component<Renderable>::Apply(Renderable &dst, const Renderable &src, bool liveTarget) {
        if (liveTarget || (!dst.model && src.model)) {
            dst.model = src.model;
        }
        if (dst.joints.empty()) dst.joints = src.joints;
    }

    Renderable::Renderable(const std::string &modelName, size_t meshIndex)
        : modelName(modelName), meshIndex(meshIndex) {
        if (modelName.empty()) {
            visibility = VisibilityMask::None;
        } else {
            model = sp::Assets().LoadGltf(modelName);
        }
    }
} // namespace ecs
