#include "assets/AssetManager.hh"
#include "core/Common.hh"
#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"
#include "ecs/EntityReferenceManager.hh"
#include "game/Scene.hh"

#include <cmath>
#include <glm/glm.hpp>

namespace sp::scripts {
    using namespace ecs;

    static sp::CVar<bool> CVarSmoothRotation("p.SmoothRotation",
        false,
        "Enable Smooth Rotation instead of Snap Rotation");

    std::array inputScripts = {
        InternalScript(
            "joystick_calibration",
            [](ScriptState &state, Lock<WriteAll> lock, Entity ent, chrono_clock::duration interval) {
                Event event;
                while (EventInput::Poll(lock, state.eventQueue, event)) {
                    if (event.name != "/action/joystick_in") continue;

                    auto data = std::get_if<glm::vec2>(&event.data);
                    if (data) {
                        float factorParamX = state.GetParam<double>("scale_x");
                        float factorParamY = state.GetParam<double>("scale_y");
                        EventBindings::SendEvent(lock,
                            "/script/joystick_out",
                            ent,
                            glm::vec2(data->x * factorParamX, data->y * factorParamY));
                    } else {
                        Errorf("Unsupported joystick_in event type: %s", event.toString());
                    }
                }
            },
            "/action/joystick_in"),
        InternalScript("relative_movement",
            [](ScriptState &state, Lock<WriteAll> lock, Entity ent, chrono_clock::duration interval) {
                if (ent.Has<SignalOutput>(lock)) {
                    auto relativeTargetName = state.GetParam<std::string>("relative_to");
                    auto upReferenceName = state.GetParam<std::string>("up_reference");
                    ecs::Name targetName(relativeTargetName, state.scope.prefix);
                    ecs::Name referenceName(upReferenceName, state.scope.prefix);
                    if (!targetName) {
                        Errorf("Relative movement target name is invalid: %s", relativeTargetName);
                        return;
                    }
                    if (!upReferenceName.empty() && !referenceName) {
                        Errorf("Relative movement up reference name is invalid: %s", upReferenceName);
                        return;
                    }

                    auto targetEntity = state.GetParam<EntityRef>("target_entity");
                    if (targetEntity.Name() != targetName) targetEntity = targetName;
                    auto referenceEntity = state.GetParam<EntityRef>("reference_entity");
                    if (referenceEntity.Name() != referenceName) referenceEntity = referenceName;

                    glm::vec3 movementInput = glm::vec3(0);
                    movementInput.x -= SignalBindings::GetSignal(lock, ent, "move_left");
                    movementInput.x += SignalBindings::GetSignal(lock, ent, "move_right");
                    movementInput.y += SignalBindings::GetSignal(lock, ent, "move_up");
                    movementInput.y -= SignalBindings::GetSignal(lock, ent, "move_down");
                    movementInput.z -= SignalBindings::GetSignal(lock, ent, "move_forward");
                    movementInput.z += SignalBindings::GetSignal(lock, ent, "move_back");

                    movementInput.x = std::clamp(movementInput.x, -1.0f, 1.0f);
                    movementInput.y = std::clamp(movementInput.y, -1.0f, 1.0f);
                    movementInput.z = std::clamp(movementInput.z, -1.0f, 1.0f);

                    glm::quat orientation = glm::identity<glm::quat>();
                    auto reference = referenceEntity.Get(lock);
                    if (reference.Has<TransformTree>(lock)) {
                        state.SetParam<EntityRef>("reference_entity", referenceEntity);

                        orientation = reference.Get<const TransformTree>(lock).GetGlobalRotation(lock);
                    }

                    glm::vec3 output = glm::vec3(0);

                    auto target = targetEntity.Get(lock);
                    if (target.Has<TransformTree>(lock)) {
                        state.SetParam<EntityRef>("target_entity", targetEntity);

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

                    auto &outputComp = ent.Get<SignalOutput>(lock);
                    outputComp.SetSignal("move_relative_x", output.x);
                    outputComp.SetSignal("move_relative_y", output.y);
                    outputComp.SetSignal("move_relative_z", output.z);
                }
            }),
        InternalScript(
            "player_rotation",
            [](ScriptState &state, Lock<WriteAll> lock, Entity ent, chrono_clock::duration interval) {
                if (ent.Has<TransformTree>(lock)) {
                    auto relativeTargetName = state.GetParam<std::string>("relative_to");
                    auto targetEntity = state.GetParam<EntityRef>("target_entity");
                    ecs::Name targetName(relativeTargetName, state.scope.prefix);
                    if (targetEntity.Name() != targetName) targetEntity = targetName;

                    auto target = targetEntity.Get(lock);
                    if (!target.Has<TransformTree>(lock)) target = ent;
                    state.SetParam<EntityRef>("target_entity", targetEntity);

                    auto &transform = ent.Get<TransformTree>(lock);
                    auto &relativeTarget = target.Get<TransformTree>(lock);

                    auto oldPosition = relativeTarget.GetGlobalTransform(lock).GetPosition();
                    bool changed = false;

                    if (CVarSmoothRotation.Get()) {
                        auto smoothRotation = SignalBindings::GetSignal(lock, ent, "smooth_rotation");
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
            },
            "/action/snap_rotate"),
        InternalScript(
            "camera_view",
            [](ScriptState &state, Lock<WriteAll> lock, Entity ent, chrono_clock::duration interval) {
                if (ent.Has<TransformTree>(lock)) {
                    Event event;
                    while (EventInput::Poll(lock, state.eventQueue, event)) {
                        if (event.name != "/script/camera_rotate") continue;

                        auto angleDiff = std::get<glm::vec2>(event.data);
                        if (SignalBindings::GetSignal(lock, ent, "interact_rotate") < 0.5) {
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
            },
            "/script/camera_rotate"),
    };
} // namespace sp::scripts
