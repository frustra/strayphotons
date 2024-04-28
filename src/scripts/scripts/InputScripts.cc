/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "assets/AssetManager.hh"
#include "common/Common.hh"
#include "common/Logging.hh"
#include "ecs/EcsImpl.hh"
#include "game/Scene.hh"
#include "input/BindingNames.hh"

#include <cmath>
#include <glm/glm.hpp>

namespace sp::scripts {
    using namespace ecs;

    struct RelativeMovement {
        EntityRef targetEntity, referenceEntity;

        void OnTick(ScriptState &state, Lock<WriteAll> lock, Entity ent, chrono_clock::duration interval) {
            glm::vec3 movementInput = glm::vec3(0);
            movementInput.x -= SignalRef(ent, "move_left").GetSignal(lock);
            movementInput.x += SignalRef(ent, "move_right").GetSignal(lock);
            movementInput.y += SignalRef(ent, "move_up").GetSignal(lock);
            movementInput.y -= SignalRef(ent, "move_down").GetSignal(lock);
            movementInput.z -= SignalRef(ent, "move_forward").GetSignal(lock);
            movementInput.z += SignalRef(ent, "move_back").GetSignal(lock);

            movementInput.x = std::clamp(movementInput.x, -1.0f, 1.0f);
            movementInput.y = std::clamp(movementInput.y, -1.0f, 1.0f);
            movementInput.z = std::clamp(movementInput.z, -1.0f, 1.0f);

            glm::quat orientation = glm::identity<glm::quat>();
            auto reference = referenceEntity.Get(lock);
            if (reference.Has<TransformTree>(lock)) {
                orientation = reference.Get<const TransformTree>(lock).GetGlobalRotation(lock);
            }

            glm::vec3 output = glm::vec3(0);

            auto target = targetEntity.Get(lock);
            if (target.Has<TransformTree>(lock)) {
                auto relativeRotation = target.Get<const TransformTree>(lock).GetGlobalRotation(lock);
                relativeRotation = glm::inverse(orientation) * relativeRotation;
                auto flatMovement = glm::vec3(movementInput.x, 0, movementInput.z);
                output = relativeRotation * flatMovement;
                if (std::abs(output.y) > 0.999) {
                    output = relativeRotation * glm::vec3(0, -output.y, 0);
                }
                output.y = 0;
                if (output != glm::vec3(0)) {
                    output = glm::normalize(output) * glm::length(flatMovement);
                }
                output.y = movementInput.y;
            } else {
                output = movementInput;
            }

            SignalRef(ent, INPUT_SIGNAL_MOVE_RELATIVE_X).SetValue(lock, output.x);
            SignalRef(ent, INPUT_SIGNAL_MOVE_RELATIVE_Y).SetValue(lock, output.y);
            SignalRef(ent, INPUT_SIGNAL_MOVE_RELATIVE_Z).SetValue(lock, output.z);
        }
    };
    StructMetadata MetadataRelativeMovement(typeid(RelativeMovement),
        "RelativeMovement",
        "",
        StructField::New("relative_to", &RelativeMovement::targetEntity),
        StructField::New("up_reference", &RelativeMovement::referenceEntity));
    InternalScript<RelativeMovement> relativeMovement("relative_movement", MetadataRelativeMovement);

    struct PlayerRotation {
        EntityRef targetEntity;
        bool enableSmoothRotation = false;

        void OnTick(ScriptState &state, Lock<WriteAll> lock, Entity ent, chrono_clock::duration interval) {
            if (!ent.Has<TransformTree>(lock)) return;

            auto target = targetEntity.Get(lock);
            if (!target.Has<TransformTree>(lock)) target = ent;

            auto &transform = ent.Get<TransformTree>(lock);
            auto &relativeTarget = target.Get<TransformTree>(lock);

            auto oldPosition = relativeTarget.GetGlobalTransform(lock).GetPosition();
            bool changed = false;

            if (enableSmoothRotation) {
                auto smoothRotation = SignalRef(ent, "smooth_rotation").GetSignal(lock);
                if (smoothRotation != 0.0f) {
                    // smooth_rotation unit is RPM
                    transform.pose.Rotate(smoothRotation * M_PI * 2.0 / 60.0 * interval.count() / 1e9,
                        glm::vec3(0, -1, 0));
                    changed = true;
                }
            } else {
                Event event;
                while (EventInput::Poll(lock, state.eventQueue, event)) {
                    if (event.name != "/action/snap_rotate") continue;

                    auto angleDiff = std::get<double>(event.data);
                    if (angleDiff != 0.0f) {
                        transform.pose.Rotate(glm::radians(angleDiff), glm::vec3(0, -1, 0));
                        changed = true;
                    }
                }
            }

            if (changed) {
                auto newPosition = relativeTarget.GetGlobalTransform(lock).GetPosition();
                transform.pose.Translate(oldPosition - newPosition);
            }
        }
    };
    StructMetadata MetadataPlayerRotation(typeid(PlayerRotation),
        "PlayerRotation",
        "",
        StructField::New("relative_to", &PlayerRotation::targetEntity),
        StructField::New("smooth_rotation", &PlayerRotation::enableSmoothRotation));
    InternalScript<PlayerRotation> playerRotation("player_rotation",
        MetadataPlayerRotation,
        false,
        "/action/snap_rotate");

    struct CameraView {
        template<typename LockType>
        void updateCamera(ScriptState &state, LockType &lock, Entity ent) {
            if (!ent.Has<TransformTree>(lock)) return;

            Event event;
            while (EventInput::Poll(lock, state.eventQueue, event)) {
                if (event.name != "/script/camera_rotate") continue;

                auto angleDiff = std::get<glm::vec2>(event.data);
                if (SignalRef(ent, "interact_rotate").GetSignal(lock) < 0.5) {
                    // Apply pitch/yaw rotations
                    auto &transform = ent.Get<TransformTree>(lock);
                    auto rotation = glm::quat(glm::vec3(0, -angleDiff.x, 0)) * transform.pose.GetRotation() *
                                    glm::quat(glm::vec3(-angleDiff.y, 0, 0));

                    auto up = rotation * glm::vec3(0, 1, 0);
                    if (up.y < 0) {
                        // Camera is turning upside-down, reset it
                        auto right = rotation * glm::vec3(1, 0, 0);
                        right.y = 0;
                        up.y = 0;
                        glm::vec3 forward = glm::cross(right, up);
                        rotation = glm::quat_cast(
                            glm::mat3(glm::normalize(right), glm::normalize(up), glm::normalize(forward)));
                    }
                    transform.pose.SetRotation(rotation);
                }
            }
        }

        void OnPhysicsUpdate(ScriptState &state, PhysicsUpdateLock lock, Entity ent, chrono_clock::duration interval) {
            updateCamera(state, lock, ent);
        }
        void OnTick(ScriptState &state, Lock<WriteAll> lock, Entity ent, chrono_clock::duration interval) {
            updateCamera(state, lock, ent);
        }
    };
    StructMetadata MetadataCameraView(typeid(CameraView), "CameraView", "");
    InternalScript<CameraView> cameraView("camera_view", MetadataCameraView, true, "/script/camera_rotate");
    InternalPhysicsScript<CameraView> physicsCameraView("physics_camera_view",
        MetadataCameraView,
        true,
        "/script/camera_rotate");
} // namespace sp::scripts
