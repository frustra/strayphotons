#include "ConsoleBindingManager.hh"

#include "console/Console.hh"
#include "ecs/EcsImpl.hh"
#include "game/Scene.hh"
#include "game/SceneManager.hh"
#include "input/BindingNames.hh"
#include "input/KeyCodes.hh"

namespace sp {
    ConsoleBindingManager::ConsoleBindingManager() {
        GetSceneManager().QueueActionAndBlock(SceneAction::AddSystemScene,
            "console",
            [](ecs::Lock<ecs::AddRemove> lock, std::shared_ptr<Scene> scene) {
                auto ent = scene->NewSystemEntity(lock, scene, consoleInputEntity.Name());
                ent.Set<ecs::FocusLayer>(lock, ecs::FocusLayer::GAME);
                ent.Set<ecs::EventInput>(lock, INPUT_EVENT_RUN_COMMAND);
                auto &script = ent.Set<ecs::Script>(lock);
                script.AddOnTick([](ecs::Lock<ecs::WriteAll> lock, ecs::Entity ent, chrono_clock::duration interval) {
                    if (ent.Has<ecs::EventInput>(lock)) {
                        ecs::Event event;
                        while (ecs::EventInput::Poll(lock, ent, INPUT_EVENT_RUN_COMMAND, event)) {
                            auto command = std::get_if<std::string>(&event.data);
                            if (command && !command->empty()) GetConsoleManager().QueueParseAndExecute(*command);
                        }
                    }
                });
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
                auto lock = ecs::World.StartTransaction<ecs::Read<ecs::Name>,
                    ecs::Write<ecs::Script, ecs::EventInput, ecs::EventBindings>>();

                auto keyboard = keyboardEntity.Get(lock);
                if (!keyboard.Has<ecs::EventBindings>(lock)) {
                    Errorf("Can't bind key without valid keyboard entity");
                    return;
                }

                Logf("Binding %s to command: %s", keyName, command);
                std::string eventName = INPUT_EVENT_KEYBOARD_KEY_BASE + keyName;
                auto &bindings = keyboard.Get<ecs::EventBindings>(lock);
                bindings.Unbind(eventName, consoleInputEntity, INPUT_EVENT_RUN_COMMAND);
                ecs::EventBindings::Binding binding;
                binding.target = consoleInputEntity;
                binding.destQueue = INPUT_EVENT_RUN_COMMAND;
                binding.setValue = command;
                bindings.Bind(eventName, binding);
            }
        } else {
            Errorf("Key \"%s\" does not exist", keyName);
        }
    }
} // namespace sp
