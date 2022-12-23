#include "ConsoleBindingManager.hh"

#include "console/Console.hh"
#include "ecs/EcsImpl.hh"
#include "ecs/EntityReferenceManager.hh"
#include "game/Scene.hh"
#include "game/SceneManager.hh"
#include "input/BindingNames.hh"
#include "input/KeyCodes.hh"

namespace sp {
    ConsoleBindingManager::ConsoleBindingManager() {
        GetSceneManager().QueueActionAndBlock(SceneAction::ApplySystemScene,
            "console",
            [this](ecs::Lock<ecs::AddRemove> lock, std::shared_ptr<Scene> scene) {
                auto ent = scene->NewSystemEntity(lock, scene, consoleInputEntity.Name());
                ent.Set<ecs::FocusLayer>(lock, ecs::FocusLayer::Game);
                ent.Set<ecs::EventInput>(lock);
                auto &script = ent.Set<ecs::Script>(lock);
                auto &scriptState = script.AddOnTick(ecs::EntityScope{scene, ecs::Name(scene->name, "")},
                    [](ecs::ScriptState &state,
                        ecs::Lock<ecs::WriteAll> lock,
                        ecs::Entity ent,
                        chrono_clock::duration interval) {
                        ecs::Event event;
                        while (ecs::EventInput::Poll(lock, state.eventQueue, event)) {
                            auto command = std::get_if<std::string>(&event.data);
                            if (command && !command->empty()) {
                                GetConsoleManager().QueueParseAndExecute(*command);
                            } else {
                                Errorf("Console binding received invalid event: %s", event.toString());
                            }
                        }
                    });
                scriptState.definition.events = {ACTION_EVENT_RUN_COMMAND};
                scriptState.definition.filterOnEvent = true;
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

                ecs::EventBinding binding;
                binding.target = consoleInputEntity;
                binding.destQueue = ACTION_EVENT_RUN_COMMAND;
                binding.setValue = command;
                bindings.Bind(eventName, binding);
            }
        } else {
            Errorf("Key \"%s\" does not exist", keyName);
        }
    }
} // namespace sp
