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

                        auto &pointer = pointers[event.source];
                        pointer.mousePos = {
                            pointOnScreen.x * io.DisplaySize.x,
                            (1.0f - pointOnScreen.y) * io.DisplaySize.y,
                        };
                    } else {
                        pointers.erase(event.source);
                    }
                }

                if (!pointers.empty()) {
                    auto mousePos = std::max_element(pointers.begin(), pointers.end(), [](auto &a, auto &b) {
                        return a.second.time < b.second.time;
                    })->second.mousePos;

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
