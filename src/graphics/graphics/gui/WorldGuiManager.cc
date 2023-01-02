#include "WorldGuiManager.hh"

#include "core/Tracing.hh"
#include "ecs/EcsImpl.hh"
#include "game/Scene.hh"
#include "game/SceneManager.hh"
#include "input/BindingNames.hh"
#include "input/KeyCodes.hh"

#include <imgui/imgui.h>

namespace sp {
    WorldGuiManager::WorldGuiManager(ecs::Entity gui, const std::string &name) : GuiContext(name), guiEntity(gui) {
        std::thread([this]() {
            auto lock = ecs::StartTransaction<ecs::Write<ecs::EventInput>>();
            auto gui = guiEntity.Get(lock);
            if (!gui.Has<ecs::EventInput>(lock)) return;

            auto &eventInput = gui.Get<ecs::EventInput>(lock);
            eventInput.Register(lock, events, INTERACT_EVENT_INTERACT_POINT);
            eventInput.Register(lock, events, INTERACT_EVENT_INTERACT_PRESS);
        }).detach();
    }

    void WorldGuiManager::DefineWindows() {
        ZoneScoped;
        for (auto &window : components) {
            ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
            ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
            ImGui::Begin(window->name.c_str(),
                nullptr,
                ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse);
            window->DefineContents();
            ImGui::End();
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

            auto screenInverseTransform = gui.Get<ecs::TransformSnapshot>(lock).GetInverse();

            ecs::Event event;
            while (ecs::EventInput::Poll(lock, events, event)) {
                auto existingState = std::find_if(pointingStack.begin(), pointingStack.end(), [&](auto &state) {
                    return state.sourceEntity == event.source;
                });

                if (event.name == INTERACT_EVENT_INTERACT_POINT) {
                    if (std::holds_alternative<ecs::Transform>(event.data)) {
                        auto pointWorld = std::get<ecs::Transform>(event.data).GetPosition();
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
                    } else if (existingState != pointingStack.end()) {
                        if (existingState->mouseDown) {
                            // Keep state if mouse was dragged off screen
                            existingState->mousePos = {-FLT_MAX, -FLT_MAX};
                        } else {
                            pointingStack.erase(existingState);
                        }
                    }
                } else if (event.name == INTERACT_EVENT_INTERACT_PRESS) {
                    if (std::holds_alternative<bool>(event.data)) {
                        bool mouseDown = std::get<bool>(event.data);
                        if (existingState != pointingStack.end()) {
                            if (!mouseDown && existingState->mousePos == glm::vec2(-FLT_MAX, -FLT_MAX)) {
                                // If mouse is released while off screen, remove the state
                                pointingStack.erase(existingState);
                            } else {
                                existingState->mouseDown = mouseDown;
                            }
                        } else {
                            Warnf("Entity %s sent press event to world gui %s without point event",
                                std::to_string(event.source),
                                guiEntity.Name().String());
                        }
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
