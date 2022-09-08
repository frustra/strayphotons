#include "WorldGuiManager.hh"

#include "core/Tracing.hh"
#include "ecs/EcsImpl.hh"
#include "game/Scene.hh"
#include "game/SceneManager.hh"
#include "input/BindingNames.hh"
#include "input/KeyCodes.hh"

#include <imgui/imgui.h>

namespace sp {
    WorldGuiManager::WorldGuiManager(ecs::Entity guiEntity, const std::string &name)
        : GuiContext(name), guiEntity(guiEntity) {}

    void WorldGuiManager::DefineWindows() {
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
            auto lock = ecs::World.StartTransaction<ecs::Read<ecs::EventInput, ecs::TransformSnapshot>>();

            auto gui = guiEntity.Get(lock);
            if (!gui.Has<ecs::TransformSnapshot>(lock)) return;

            auto screenInverseTransform = gui.Get<ecs::TransformSnapshot>(lock).GetInverse();

            io.MousePos.x = io.MousePos.y = -FLT_MAX;

            if (gui.Has<ecs::EventInput>(lock)) {
                ecs::Event event;
                while (ecs::EventInput::Poll(lock, gui, INTERACT_EVENT_INTERACT_POINT, event)) {
                    if (std::holds_alternative<ecs::Transform>(event.data)) {
                        auto pointWorld = std::get<ecs::Transform>(event.data).GetPosition();
                        auto pointOnScreen = screenInverseTransform * glm::vec4(pointWorld, 1);
                        pointOnScreen += 0.5f;

                        glm::vec2 mousePos = {
                            pointOnScreen.x * io.DisplaySize.x,
                            (1.0f - pointOnScreen.y) * io.DisplaySize.y,
                        };

                        auto existingPos = std::find_if(pointingStack.begin(), pointingStack.end(), [&](auto &state) {
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
                }

                if (!pointingStack.empty()) {
                    auto mousePos = pointingStack.back().mousePos;

                    io.MousePos.x = mousePos.x;
                    io.MousePos.y = mousePos.y;
                }

                while (ecs::EventInput::Poll(lock, gui, INTERACT_EVENT_INTERACT_PRESS, event)) {
                    if (std::holds_alternative<bool>(event.data)) io.MouseDown[0] = std::get<bool>(event.data);
                }
            } else {
                io.MouseDown[0] = false;
            }
        }
    }

} // namespace sp
