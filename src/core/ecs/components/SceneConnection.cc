/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "SceneConnection.hh"

#include "ecs/EcsImpl.hh"
#include "game/Scene.hh"

#include <algorithm>
#include <picojson/picojson.h>

namespace ecs {
    template<>
    void Component<SceneConnection>::Apply(SceneConnection &dst, const SceneConnection &src, bool liveTarget) {
        if (liveTarget) {
            dst = src;
        } else {
            for (auto &[scene, signals] : src.scenes) {
                auto &scenes = dst.scenes[scene];
                scenes.insert(scenes.end(), signals.begin(), signals.end());
            }
        }
    }
} // namespace ecs
