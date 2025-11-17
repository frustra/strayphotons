/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "ConsoleBindingManager.hh"

#include "console/Console.hh"
#include "ecs/EcsImpl.hh"
#include "ecs/ScriptImpl.hh"
#include "game/Scene.hh"
#include "game/SceneManager.hh"
#include "input/BindingNames.hh"
#include "input/KeyCodes.hh"

namespace sp {
    ConsoleBindingManager::ConsoleBindingManager() {
        GetSceneManager().QueueActionAndBlock(SceneAction::ApplySystemScene,
            "console",
            [this](ecs::Lock<ecs::AddRemove> lock, std::shared_ptr<Scene> scene) {
                ecs::Entity ent = scene->NewSystemEntity(lock, scene, consoleInputEntity.Name());
                ent.Set<ecs::EventInput>(lock);
                auto &scripts = ent.Set<ecs::Scripts>(lock);
                auto eventScript = ecs::CreateLogicScript<ecs::Lock<ecs::Read<ecs::EventInput>>>(
                    [](ecs::ScriptState &state, auto lock, ecs::Entity ent, chrono_clock::duration interval) {
                        ecs::Event event;
                        while (ecs::EventInput::Poll(lock, state.eventQueue, event)) {
                            auto *command = ecs::EventData::TryGet<ecs::EventString>(event.data);
                            if (command && !command->empty()) {
                                GetConsoleManager().QueueParseAndExecute(*command);
                            } else {
                                Errorf("Console binding received invalid event: %s", event.ToString());
                            }
                        }
                    });
                eventScript.events = {ACTION_EVENT_RUN_COMMAND};
                eventScript.filterOnEvent = true;
                scripts.AddScript(ecs::Name(scene->data->name, ""), eventScript);
            });

        funcs.Register(this, "bind", "Bind a key to a command", &ConsoleBindingManager::BindKey);
    }

    void ConsoleBindingManager::BindKey(string keyName, string command) {
        to_lower(keyName);
        auto it = UserBindingAliases.find(keyName);
        if (it != UserBindingAliases.end()) keyName = it->second;

        KeyCode key = KEY_INVALID;
        for (auto &[keyCode, name] : KeycodeNameLookup) {
            if (name == keyName) {
                key = (KeyCode)keyCode;
                break;
            }
        }
        if (key != KEY_INVALID) {
            trim(command);

            {
                auto lock = ecs::StartTransaction<ecs::Write<ecs::EventBindings>>();

                auto keyboard = keyboardEntity.Get(lock);
                if (!keyboard.Has<ecs::EventBindings>(lock)) {
                    Errorf("Can't bind key without valid keyboard entity");
                    return;
                }

                Logf("Binding %s to command: %s", keyName, command);
                std::string eventName = INPUT_EVENT_KEYBOARD_KEY_BASE + keyName;
                auto &bindings = keyboard.Get<ecs::EventBindings>(lock);
                bindings.Unbind(eventName, consoleInputEntity, ACTION_EVENT_RUN_COMMAND);

                auto &binding = bindings.Bind(eventName, consoleInputEntity, ACTION_EVENT_RUN_COMMAND);
                binding.actions.setValue = command;
            }
        } else {
            Errorf("Key \"%s\" does not exist", keyName);
        }
    }
} // namespace sp
