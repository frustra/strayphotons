/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "PhysicsJoints.hh"

#include "assets/AssetManager.hh"
#include "assets/JsonHelpers.hh"
#include "common/Common.hh"
#include "ecs/EcsImpl.hh"

namespace ecs {
    template<>
    void Component<PhysicsJoints>::Apply(PhysicsJoints &dst, const PhysicsJoints &src, bool liveTarget) {
        for (auto &joint : src.joints) {
            if (!sp::contains(dst.joints, joint)) dst.joints.emplace_back(joint);
        }
    }
} // namespace ecs
