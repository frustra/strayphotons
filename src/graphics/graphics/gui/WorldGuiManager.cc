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
            if (!gui.Has<ecs::TransformSnapshot>(lock)) return;

            auto screenInverseTransform = gui.Get<ecs::TransformSnapshot>(lock).GetInverse();

            io.MousePos.x = io.MousePos.y = -FLT_MAX;

            if (gui.Has<ecs::EventInput>(lock)) {
                ecs::Event event;
                while (ecs::EventInput::Poll(lock, events, event)) {
                    if (event.name == INTERACT_EVENT_INTERACT_POINT) {

                        if (std::holds_alternative<ecs::Transform>(event.data)) {
                            auto pointWorld = std::get<ecs::Transform>(event.data).GetPosition();
                            auto pointOnScreen = screenInverseTransform * glm::vec4(pointWorld, 1);
                            pointOnScreen += 0.5f;

                            glm::vec2 mousePos = {
                                pointOnScreen.x * io.DisplaySize.x,
                                (1.0f - pointOnScreen.y) * io.DisplaySize.y,
                            };

                            auto existingPos = std::find_if(pointingStack.begin(),
                                pointingStack.end(),
                                [&](auto &state) {
                                    return state.sourceEntity == event.source;
                                });

                            if (existingPos != pointingStack.end()) {
                                existingPos->mousePos = mousePos;
                            } else {
                                pointingStack.emplace_back(PointingState{event.source, mousePos});
                            }
                        } else {
                            erase_if(pointingStack, [&](auto &state) {
                                return state.sourceEntity == event.source;
                            });
                        }
                    } else if (event.name == INTERACT_EVENT_INTERACT_PRESS) {
                        if (std::holds_alternative<bool>(event.data)) io.MouseDown[0] = std::get<bool>(event.data);
                    }
                }

                if (!pointingStack.empty()) {
                    auto mousePos = pointingStack.back().mousePos;

                    io.MousePos.x = mousePos.x;
                    io.MousePos.y = mousePos.y;
                }
            } else {
                io.MouseDown[0] = false;
            }
        }
    }

} // namespace sp
