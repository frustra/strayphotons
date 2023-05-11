#include "SystemGuiManager.hh"

#include "core/Tracing.hh"
#include "ecs/EcsImpl.hh"
#include "game/GameEntities.hh"
#include "game/Scene.hh"
#include "game/SceneManager.hh"
#include "graphics/gui/ImGuiKeyCodes.hh"
#include "input/BindingNames.hh"

#include <imgui/imgui.h>

namespace sp {
    SystemGuiManager::SystemGuiManager(const std::string &name) : GuiContext(name) {
        guiEntity = ecs::Name("gui", name);

        GetSceneManager().QueueActionAndBlock(SceneAction::ApplySystemScene,
            "gui",
            [this](ecs::Lock<ecs::AddRemove> lock, std::shared_ptr<Scene> scene) {
                auto ent = scene->NewSystemEntity(lock, scene, guiEntity.Name());
                ent.Set<ecs::EventInput>(lock);
            });

        {
            auto lock = ecs::StartTransaction<ecs::Write<ecs::EventInput>>();
            auto gui = guiEntity.Get(lock);
            Assertf(gui.Has<ecs::EventInput>(lock),
                "System Gui entity has no EventInput: %s",
                guiEntity.Name().String());
            auto &eventInput = gui.Get<ecs::EventInput>(lock);
            eventInput.Register(lock, events, INPUT_EVENT_MENU_SCROLL);
            eventInput.Register(lock, events, INPUT_EVENT_MENU_CURSOR);
            eventInput.Register(lock, events, INPUT_EVENT_MENU_PRIMARY_TRIGGER);
            eventInput.Register(lock, events, INPUT_EVENT_MENU_SECONDARY_TRIGGER);
            eventInput.Register(lock, events, INPUT_EVENT_MENU_TEXT_INPUT);
            eventInput.Register(lock, events, INPUT_EVENT_MENU_KEY_DOWN);
            eventInput.Register(lock, events, INPUT_EVENT_MENU_KEY_UP);
        }
    }

    void SystemGuiManager::BeforeFrame() {
        ZoneScoped;
        GuiContext::BeforeFrame();
        ImGuiIO &io = ImGui::GetIO();

        {
            auto lock = ecs::StartTransaction<ecs::Read<ecs::EventInput>>();

            ecs::Event event;
            while (ecs::EventInput::Poll(lock, events, event)) {
                if (event.name == INPUT_EVENT_MENU_SCROLL) {
                    auto &scroll = std::get<glm::vec2>(event.data);
                    io.AddMouseWheelEvent(scroll.x, scroll.y);
                } else if (event.name == INPUT_EVENT_MENU_CURSOR) {
                    auto &pos = std::get<glm::vec2>(event.data);
                    io.AddMousePosEvent(pos.x, pos.y);
                } else if (event.name == INPUT_EVENT_MENU_PRIMARY_TRIGGER) {
                    auto &down = std::get<bool>(event.data);
                    io.AddMouseButtonEvent(ImGuiMouseButton_Left, down);
                } else if (event.name == INPUT_EVENT_MENU_SECONDARY_TRIGGER) {
                    auto &down = std::get<bool>(event.data);
                    io.AddMouseButtonEvent(ImGuiMouseButton_Right, down);
                } else if (event.name == INPUT_EVENT_MENU_TEXT_INPUT) {
                    io.AddInputCharacter(std::get<char>(event.data));
                } else if (event.name == INPUT_EVENT_MENU_KEY_DOWN) {
                    auto &keyName = std::get<std::string>(event.data);
                    auto keyCode = NameKeycodeLookup.find(keyName);
                    if (keyCode != NameKeycodeLookup.end()) {
                        if (keyCode->second == KEY_LEFT_CONTROL || keyCode->second == KEY_RIGHT_CONTROL) {
                            io.AddKeyEvent(ImGuiMod_Ctrl, true);
                        } else if (keyCode->second == KEY_LEFT_SHIFT || keyCode->second == KEY_RIGHT_SHIFT) {
                            io.AddKeyEvent(ImGuiMod_Shift, true);
                        } else if (keyCode->second == KEY_LEFT_ALT || keyCode->second == KEY_RIGHT_ALT) {
                            io.AddKeyEvent(ImGuiMod_Alt, true);
                        } else if (keyCode->second == KEY_LEFT_SUPER || keyCode->second == KEY_RIGHT_SUPER) {
                            io.AddKeyEvent(ImGuiMod_Super, true);
                        }
                        auto imguiKey = ImGuiKeyMapping.find(keyCode->second);
                        if (imguiKey != ImGuiKeyMapping.end()) {
                            io.AddKeyEvent(imguiKey->second, true);
                        }
                    }
                } else if (event.name == INPUT_EVENT_MENU_KEY_UP) {
                    auto &keyName = std::get<std::string>(event.data);
                    auto keyCode = NameKeycodeLookup.find(keyName);
                    if (keyCode != NameKeycodeLookup.end()) {
                        if (keyCode->second == KEY_LEFT_CONTROL || keyCode->second == KEY_RIGHT_CONTROL) {
                            io.AddKeyEvent(ImGuiMod_Ctrl, false);
                        } else if (keyCode->second == KEY_LEFT_SHIFT || keyCode->second == KEY_RIGHT_SHIFT) {
                            io.AddKeyEvent(ImGuiMod_Shift, false);
                        } else if (keyCode->second == KEY_LEFT_ALT || keyCode->second == KEY_RIGHT_ALT) {
                            io.AddKeyEvent(ImGuiMod_Alt, false);
                        } else if (keyCode->second == KEY_LEFT_SUPER || keyCode->second == KEY_RIGHT_SUPER) {
                            io.AddKeyEvent(ImGuiMod_Super, false);
                        }
                        auto imguiKey = ImGuiKeyMapping.find(keyCode->second);
                        if (imguiKey != ImGuiKeyMapping.end()) {
                            io.AddKeyEvent(imguiKey->second, false);
                        }
                    }
                }
            }
        }
    }

} // namespace sp
