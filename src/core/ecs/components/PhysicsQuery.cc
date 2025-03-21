/*
 * Stray Photons - Copyright (C) 2024 Jacob Wirth
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "PhysicsQuery.hh"

#include "assets/AssetManager.hh"
#include "common/Logging.hh"
#include "ecs/EcsImpl.hh"

namespace ecs {
    template<>
    void Component<PhysicsQuery>::Apply(PhysicsQuery &dst, const PhysicsQuery &src, bool liveTarget) {
        if (liveTarget && dst.queries.empty()) dst.queries = src.queries;
    }
} // namespace ecs
