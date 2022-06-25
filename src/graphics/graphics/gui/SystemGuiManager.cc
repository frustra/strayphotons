#include "SystemGuiManager.hh"

#include "core/Tracing.hh"
#include "ecs/EcsImpl.hh"
#include "ecs/EntityReferenceManager.hh"
#include "game/Scene.hh"
#include "game/SceneManager.hh"
#include "input/BindingNames.hh"
#include "input/KeyCodes.hh"

#include <imgui/imgui.h>

namespace sp {
    SystemGuiManager::SystemGuiManager(const std::string &name, ecs::FocusLayer layer)
        : GuiContext(name), focusLayer(layer) {
        guiEntity = ecs::Name("gui", name);

        GetSceneManager().QueueActionAndBlock(SceneAction::ApplySystemScene,
            "gui",
            [this, layer](ecs::Lock<ecs::AddRemove> lock, std::shared_ptr<Scene> scene) {
                auto ent = scene->NewSystemEntity(lock, scene, guiEntity.Name());
                ent.Set<ecs::FocusLayer>(lock, layer);
                ent.Set<ecs::EventInput>(lock, INPUT_EVENT_MENU_SCROLL, INPUT_EVENT_MENU_TEXT_INPUT);
                ent.Set<ecs::Gui>(lock).target = this;

                auto &signalBindings = ent.Set<ecs::SignalBindings>(lock);
                signalBindings.Bind(INPUT_SIGNAL_MENU_PRIMARY_TRIGGER, playerEntity, INPUT_SIGNAL_MENU_PRIMARY_TRIGGER);
                signalBindings.Bind(INPUT_SIGNAL_MENU_SECONDARY_TRIGGER,
                    playerEntity,
                    INPUT_SIGNAL_MENU_SECONDARY_TRIGGER);
                signalBindings.Bind(INPUT_SIGNAL_MENU_CURSOR_X, playerEntity, INPUT_SIGNAL_MENU_CURSOR_X);
                signalBindings.Bind(INPUT_SIGNAL_MENU_CURSOR_Y, playerEntity, INPUT_SIGNAL_MENU_CURSOR_Y);
            });
    }

    void SystemGuiManager::BeforeFrame() {
        ZoneScoped;
        GuiContext::BeforeFrame();
        ImGuiIO &io = ImGui::GetIO();

        {
            auto lock = ecs::World.StartTransaction<ecs::ReadSignalsLock, ecs::Read<ecs::EventInput>>();

            bool hasFocus = true;
            if (lock.Has<ecs::FocusLock>()) {
                auto &focusLock = lock.Get<ecs::FocusLock>();
                hasFocus = focusLock.HasPrimaryFocus(focusLayer);
            }

            auto keyboard = keyboardEntity.Get(lock);
            if (keyboard.Has<ecs::SignalOutput>(lock)) {
                auto &signalOutput = keyboard.Get<ecs::SignalOutput>(lock);
                for (int keyCode = KEY_SPACE; keyCode < KEY_BACKTICK; keyCode++) {
                    auto keyName = KeycodeNameLookup.find(keyCode);
                    if (keyName != KeycodeNameLookup.end()) {
                        io.KeysDown[keyCode] = hasFocus &&
                                               signalOutput.GetSignal(INPUT_SIGNAL_KEYBOARD_KEY_BASE + keyName->second);
                    }
                }
                for (int keyCode = KEY_ESCAPE; keyCode < KEY_RIGHT_SUPER; keyCode++) {
                    auto keyName = KeycodeNameLookup.find(keyCode);
                    if (keyName != KeycodeNameLookup.end()) {
                        io.KeysDown[keyCode] = hasFocus &&
                                               signalOutput.GetSignal(INPUT_SIGNAL_KEYBOARD_KEY_BASE + keyName->second);
                    }
                }

                io.KeyCtrl = io.KeysDown[KEY_LEFT_CONTROL] || io.KeysDown[KEY_RIGHT_CONTROL];
                io.KeyShift = io.KeysDown[KEY_LEFT_SHIFT] || io.KeysDown[KEY_RIGHT_SHIFT];
                io.KeyAlt = io.KeysDown[KEY_LEFT_ALT] || io.KeysDown[KEY_RIGHT_ALT];
                io.KeySuper = io.KeysDown[KEY_LEFT_SUPER] || io.KeysDown[KEY_RIGHT_SUPER];
            }

            io.MouseWheel = 0.0f;
            io.MouseWheelH = 0.0f;
            if (hasFocus) {
                auto gui = guiEntity.Get(lock);
                if (gui.Has<ecs::EventInput>(lock)) {
                    ecs::Event event;
                    while (ecs::EventInput::Poll(lock, gui, INPUT_EVENT_MENU_SCROLL, event)) {
                        auto &scroll = std::get<glm::vec2>(event.data);
                        io.MouseWheel += scroll.y;
                        io.MouseWheelH += scroll.x;
                    }
                    while (ecs::EventInput::Poll(lock, gui, INPUT_EVENT_MENU_TEXT_INPUT, event)) {
                        io.AddInputCharacter(std::get<char>(event.data));
                    }
                }

                io.MouseDown[0] = ecs::SignalBindings::GetSignal(lock, gui, INPUT_SIGNAL_MENU_PRIMARY_TRIGGER) >= 0.5;
                io.MouseDown[1] = ecs::SignalBindings::GetSignal(lock, gui, INPUT_SIGNAL_MENU_SECONDARY_TRIGGER) >= 0.5;

                io.MousePos.x = ecs::SignalBindings::GetSignal(lock, gui, INPUT_SIGNAL_MENU_CURSOR_X);
                io.MousePos.y = ecs::SignalBindings::GetSignal(lock, gui, INPUT_SIGNAL_MENU_CURSOR_Y);
            }
        }
    }

} // namespace sp
