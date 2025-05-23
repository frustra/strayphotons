/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "SystemGuiManager.hh"

#include "common/Tracing.hh"
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
                    if (!std::holds_alternative<glm::vec2>(event.data)) {
                        Warnf("System GUI received unexpected event data: %s, expected vec2", event.ToString());
                        continue;
                    }
                    auto &scroll = std::get<glm::vec2>(event.data);
                    io.AddMouseWheelEvent(scroll.x, scroll.y);
                } else if (event.name == INPUT_EVENT_MENU_CURSOR) {
                    if (!std::holds_alternative<glm::vec2>(event.data)) {
                        Warnf("System GUI received unexpected event data: %s, expected vec2", event.ToString());
                        continue;
                    }
                    auto &pos = std::get<glm::vec2>(event.data);
                    io.AddMousePosEvent(pos.x / io.DisplayFramebufferScale.x, pos.y / io.DisplayFramebufferScale.y);
                } else if (event.name == INPUT_EVENT_MENU_PRIMARY_TRIGGER) {
                    if (!std::holds_alternative<bool>(event.data)) {
                        Warnf("System GUI received unexpected event data: %s, expected bool", event.ToString());
                        continue;
                    }
                    auto &down = std::get<bool>(event.data);
                    io.AddMouseButtonEvent(ImGuiMouseButton_Left, down);
                } else if (event.name == INPUT_EVENT_MENU_SECONDARY_TRIGGER) {
                    if (!std::holds_alternative<bool>(event.data)) {
                        Warnf("System GUI received unexpected event data: %s, expected bool", event.ToString());
                        continue;
                    }
                    auto &down = std::get<bool>(event.data);
                    io.AddMouseButtonEvent(ImGuiMouseButton_Right, down);
                } else if (event.name == INPUT_EVENT_MENU_TEXT_INPUT) {
                    if (!std::holds_alternative<unsigned int>(event.data)) {
                        Warnf("System GUI received unexpected event data: %s, expected uint", event.ToString());
                        continue;
                    }
                    io.AddInputCharacter(std::get<unsigned int>(event.data));
                } else if (event.name == INPUT_EVENT_MENU_KEY_DOWN) {
                    if (!std::holds_alternative<int>(event.data)) {
                        Warnf("System GUI received unexpected event data: %s, expected int", event.ToString());
                        continue;
                    }
                    auto &keyCode = (KeyCode &)std::get<int>(event.data);
                    if (keyCode == KEY_LEFT_CONTROL || keyCode == KEY_RIGHT_CONTROL) {
                        io.AddKeyEvent(ImGuiMod_Ctrl, true);
                    } else if (keyCode == KEY_LEFT_SHIFT || keyCode == KEY_RIGHT_SHIFT) {
                        io.AddKeyEvent(ImGuiMod_Shift, true);
                    } else if (keyCode == KEY_LEFT_ALT || keyCode == KEY_RIGHT_ALT) {
                        io.AddKeyEvent(ImGuiMod_Alt, true);
                    } else if (keyCode == KEY_LEFT_SUPER || keyCode == KEY_RIGHT_SUPER) {
                        io.AddKeyEvent(ImGuiMod_Super, true);
                    }
                    auto imguiKey = ImGuiKeyMapping.find(keyCode);
                    if (imguiKey != ImGuiKeyMapping.end()) {
                        io.AddKeyEvent(imguiKey->second, true);
                    }
                } else if (event.name == INPUT_EVENT_MENU_KEY_UP) {
                    if (!std::holds_alternative<int>(event.data)) {
                        Warnf("System GUI received unexpected event data: %s, expected int", event.ToString());
                        continue;
                    }
                    auto &keyCode = (KeyCode &)std::get<int>(event.data);
                    if (keyCode == KEY_LEFT_CONTROL || keyCode == KEY_RIGHT_CONTROL) {
                        io.AddKeyEvent(ImGuiMod_Ctrl, false);
                    } else if (keyCode == KEY_LEFT_SHIFT || keyCode == KEY_RIGHT_SHIFT) {
                        io.AddKeyEvent(ImGuiMod_Shift, false);
                    } else if (keyCode == KEY_LEFT_ALT || keyCode == KEY_RIGHT_ALT) {
                        io.AddKeyEvent(ImGuiMod_Alt, false);
                    } else if (keyCode == KEY_LEFT_SUPER || keyCode == KEY_RIGHT_SUPER) {
                        io.AddKeyEvent(ImGuiMod_Super, false);
                    }
                    auto imguiKey = ImGuiKeyMapping.find(keyCode);
                    if (imguiKey != ImGuiKeyMapping.end()) {
                        io.AddKeyEvent(imguiKey->second, false);
                    }
                }
            }
        }
    }

} // namespace sp
