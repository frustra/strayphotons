/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "ecs/EntityRef.hh"

#include <openvr.h>
#include <string>
#include <vector>

namespace sp::xr {
    class OpenVrSystem;

    class InputBindings {
    public:
        InputBindings(OpenVrSystem &vrSystem, std::string actionManifestPath);
        ~InputBindings();

        void Frame();

    private:
        OpenVrSystem &vrSystem;

        struct Action {
            enum class DataType {
                Bool,
                Vec1,
                Vec2,
                Vec3,
                Haptic,
                Pose,
                Skeleton,
            };

            std::string name;
            vr::VRActionHandle_t handle;
            ecs::EntityRef poseEntity;
            std::vector<ecs::EntityRef> boneEntities;
            std::vector<vr::BoneIndex_t> boneHierarchy;
            ecs::EventQueueRef eventQueue;
            DataType type;

            Action() {}
            Action(std::string &name, vr::VRActionHandle_t handle, DataType type)
                : name(name), handle(handle), type(type) {}
        };

        struct ActionSet {
            std::string name;
            vr::VRActionSetHandle_t handle;
            std::vector<Action> actions;

            ActionSet() {}
            ActionSet(std::string &name, vr::VRActionSetHandle_t handle) : name(name), handle(handle) {}
        };

        std::vector<ActionSet> actionSets;
        ecs::EntityRef outputEntity = ecs::Name("output", "haptics");
    };
} // namespace sp::xr
