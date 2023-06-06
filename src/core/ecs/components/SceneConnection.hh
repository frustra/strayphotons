/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "ecs/Components.hh"
#include "ecs/SignalExpression.hh"

#include <robin_hood.h>
#include <string>
#include <vector>

namespace ecs {
    struct SceneConnection {
        // Load a scene if any of the signal expressions provded evaluate to true
        robin_hood::unordered_map<std::string, std::vector<SignalExpression>> scenes;

        SceneConnection() {}
        SceneConnection(std::string scene, const SignalExpression &expr) {
            scenes.emplace(scene, std::vector<SignalExpression>{expr});
        }
    };

    static StructMetadata MetadataSceneConnection(typeid(SceneConnection),
        "scene_connection",
        "",
        StructField::New(&SceneConnection::scenes, ~FieldAction::AutoApply));
    static Component<SceneConnection> ComponentSceneConnection(MetadataSceneConnection);

    template<>
    void Component<SceneConnection>::Apply(SceneConnection &dst, const SceneConnection &src, bool liveTarget);
} // namespace ecs
