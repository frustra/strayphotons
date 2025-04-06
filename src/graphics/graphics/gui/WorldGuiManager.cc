/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "WorldGuiManager.hh"

#include "common/Tracing.hh"
#include "ecs/EcsImpl.hh"
#include "game/Scene.hh"
#include "game/SceneManager.hh"
#include "input/BindingNames.hh"
#include "input/KeyCodes.hh"

#include <imgui/imgui.h>

namespace sp {
    WorldGuiManager::WorldGuiManager(ecs::Entity gui, const std::string &name) : GuiContext(name), guiEntity(gui) {
        ecs::QueueTransaction<ecs::Write<ecs::EventInput>>([this](auto &lock) {
            ecs::Entity gui = guiEntity.Get(lock);
            if (!gui.Has<ecs::EventInput>(lock)) return;

            auto &eventInput = gui.Get<ecs::EventInput>(lock);
            eventInput.Register(lock, events, INTERACT_EVENT_INTERACT_POINT);
            eventInput.Register(lock, events, INTERACT_EVENT_INTERACT_PRESS);
        });
    }

    void WorldGuiManager::DefineWindows() {
        ZoneScoped;
        ImGuiIO &io = ImGui::GetIO();
        for (auto &component : components) {
            auto *window = dynamic_cast<GuiWindow *>(component.get());

            int flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse;
            if (window) {
                window->PreDefine();
                flags |= window->flags;
            }
            ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
            ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, io.DisplaySize.y));
            ImGui::Begin(component->name.c_str(), nullptr, flags);
            component->DefineContents();
            ImGui::End();
            if (window) window->PostDefine();
        }
    }

    void WorldGuiManager::BeforeFrame() {
        ZoneScoped;
        GuiContext::BeforeFrame();
        ImGuiIO &io = ImGui::GetIO();

        {
            auto lock = ecs::StartTransaction<ecs::Read<ecs::EventInput, ecs::TransformSnapshot>>();

            auto gui = guiEntity.Get(lock);
            if (!gui.Has<ecs::TransformSnapshot, ecs::EventInput>(lock)) return;

            auto screenInverseTransform = gui.Get<ecs::TransformSnapshot>(lock).globalPose.GetInverse();

            ecs::Event event;
            while (ecs::EventInput::Poll(lock, events, event)) {
                auto existingState = std::find_if(pointingStack.begin(), pointingStack.end(), [&](auto &state) {
                    return state.sourceEntity == event.source;
                });

                if (event.name == INTERACT_EVENT_INTERACT_POINT) {
                    if (std::holds_alternative<ecs::Transform>(event.data)) {
                        auto &pointWorld = std::get<ecs::Transform>(event.data).GetPosition();
                        auto pointOnScreen = screenInverseTransform * glm::vec4(pointWorld, 1);
                        pointOnScreen += 0.5f;

                        glm::vec2 mousePos = {
                            pointOnScreen.x * io.DisplaySize.x,
                            (1.0f - pointOnScreen.y) * io.DisplaySize.y,
                        };

                        if (existingState != pointingStack.end()) {
                            existingState->mousePos = mousePos;
                        } else {
                            pointingStack.emplace_back(PointingState{event.source, mousePos});
                        }
                    } else if (std::holds_alternative<glm::vec2>(event.data)) {
                        glm::vec2 &mousePos = std::get<glm::vec2>(event.data);
                        if (existingState != pointingStack.end()) {
                            existingState->mousePos = mousePos;
                        } else {
                            pointingStack.emplace_back(PointingState{event.source, mousePos});
                        }
                    } else if (std::holds_alternative<bool>(event.data)) {
                        if (existingState != pointingStack.end()) {
                            if (existingState->mouseDown) {
                                // Keep state if mouse was dragged off screen
                                existingState->mousePos = {-FLT_MAX, -FLT_MAX};
                            } else {
                                pointingStack.erase(existingState);
                            }
                        }
                    } else {
                        Warnf("World GUI received unexpected event data: %s, expected Transform, vec2, or bool",
                            event.ToString());
                    }
                } else if (event.name == INTERACT_EVENT_INTERACT_PRESS) {
                    if (std::holds_alternative<bool>(event.data)) {
                        bool mouseDown = std::get<bool>(event.data);
                        if (existingState != pointingStack.end()) {
                            if (mouseDown != existingState->mouseDown) {
                                // Send previous mouse event immediately so fast clicks aren't missed
                                io.AddMousePosEvent(existingState->mousePos.x, existingState->mousePos.y);
                                io.AddMouseButtonEvent(ImGuiMouseButton_Left, existingState->mouseDown);
                                existingState->mouseDown = mouseDown;
                            }
                            if (!mouseDown && existingState->mousePos == glm::vec2(-FLT_MAX, -FLT_MAX)) {
                                // Remove the state if the mouse is released off screen
                                pointingStack.erase(existingState);
                            }
                        } else {
                            Warnf("Entity %s sent press event to world gui %s without point event",
                                std::to_string(event.source),
                                guiEntity.Name().String());
                        }
                    } else {
                        Warnf("World GUI received unexpected event data: %s, expected bool", event.ToString());
                        continue;
                    }
                }
            }

            if (!pointingStack.empty()) {
                auto &state = pointingStack.back();
                io.AddMousePosEvent(state.mousePos.x, state.mousePos.y);
                io.AddMouseButtonEvent(ImGuiMouseButton_Left, state.mouseDown);
            } else {
                io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
                io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
            }
        }
    }

} // namespace sp
