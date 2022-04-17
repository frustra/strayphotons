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

    std::array inputScripts = {
        InternalScript("joystick_calibration",
            [](ScriptState &state, Lock<WriteAll> lock, Entity ent, chrono_clock::duration interval) {
                if (ent.Has<Name, EventInput>(lock)) {
                    Event event;
                    while (EventInput::Poll(lock, ent, "/action/joystick_in", event)) {
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
                }
            }),
        InternalScript("relative_movement",
            [](ScriptState &state, Lock<WriteAll> lock, Entity ent, chrono_clock::duration interval) {
                if (ent.Has<SignalOutput>(lock)) {
                    auto fullTargetName = state.GetParam<std::string>("relative_to");
                    ecs::Name targetName;
                    if (targetName.Parse(fullTargetName, state.scope.prefix)) {
                        auto targetEntity = state.GetParam<EntityRef>("target_entity");
                        if (targetEntity.Name() != targetName) targetEntity = targetName;

                        auto target = targetEntity.Get(lock);
                        if (target) {
                            state.SetParam<EntityRef>("target_entity", targetEntity);

                            glm::vec3 movement = glm::vec3(0);
                            movement.z -= SignalBindings::GetSignal(lock, ent, "move_forward");
                            movement.z += SignalBindings::GetSignal(lock, ent, "move_back");
                            movement.x -= SignalBindings::GetSignal(lock, ent, "move_left");
                            movement.x += SignalBindings::GetSignal(lock, ent, "move_right");
                            float vertical = SignalBindings::GetSignal(lock, ent, "move_up");
                            vertical -= SignalBindings::GetSignal(lock, ent, "move_down");

                            movement.x = std::clamp(movement.x, -1.0f, 1.0f);
                            movement.z = std::clamp(movement.z, -1.0f, 1.0f);
                            vertical = std::clamp(vertical, -1.0f, 1.0f);

                            if (target.Has<TransformTree>(lock)) {
                                auto parentRotation = target.Get<const TransformTree>(lock).GetGlobalRotation(lock);
                                movement = parentRotation * movement;
                                if (std::abs(movement.y) > 0.999) {
                                    movement = parentRotation * glm::vec3(0, -movement.y, 0);
                                }
                                movement.y = 0;
                            }

                            auto &outputComp = ent.Get<SignalOutput>(lock);
                            outputComp.SetSignal("move_world_x", movement.x);
                            outputComp.SetSignal("move_world_y", vertical);
                            outputComp.SetSignal("move_world_z", movement.z);
                        }
                    } else {
                        Errorf("Relative target name is invalid: %s", fullTargetName);
                    }
                }
            }),
        InternalScript("snap_rotate",
            [](ScriptState &state, Lock<WriteAll> lock, Entity ent, chrono_clock::duration interval) {
                if (ent.Has<EventInput, TransformTree>(lock)) {
                    auto fullTargetName = state.GetParam<std::string>("relative_to");
                    auto targetEntity = state.GetParam<EntityRef>("target_entity");
                    ecs::Name targetName;
                    targetName.Parse(fullTargetName, state.scope.prefix);
                    if (targetEntity.Name() != targetName) targetEntity = targetName;

                    auto target = targetEntity.Get(lock);
                    if (!target.Has<TransformTree>(lock)) target = ent;
                    state.SetParam<EntityRef>("target_entity", targetEntity);

                    auto &transform = ent.Get<TransformTree>(lock);
                    auto &relativeTarget = target.Get<TransformTree>(lock);

                    auto oldPosition = relativeTarget.GetGlobalTransform(lock).GetPosition();

                    Event event;
                    while (EventInput::Poll(lock, ent, "/action/snap_rotate", event)) {
                        auto angleDiff = std::get<double>(event.data);

                        transform.pose.Rotate(glm::radians(angleDiff), glm::vec3(0, -1, 0));
                    }

                    auto newPosition = relativeTarget.GetGlobalTransform(lock).GetPosition();
                    transform.pose.Translate(oldPosition - newPosition);
                }
            }),
        InternalScript("camera_view",
            [](ScriptState &state, Lock<WriteAll> lock, Entity ent, chrono_clock::duration interval) {
                if (ent.Has<EventInput, TransformTree>(lock)) {
                    Event event;
                    while (EventInput::Poll(lock, ent, "/script/camera_rotate", event)) {
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
            }),
    };
} // namespace sp::scripts
