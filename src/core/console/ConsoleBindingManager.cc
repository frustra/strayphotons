#include "ConsoleBindingManager.hh"

#include "console/Console.hh"
#include "ecs/EcsImpl.hh"
#include "game/SceneManager.hh"
#include "input/BindingNames.hh"
#include "input/KeyCodes.hh"

namespace sp {
    ConsoleBindingManager::ConsoleBindingManager() {
        GetSceneManager().AddToSystemScene([](ecs::Lock<ecs::AddRemove> lock, std::shared_ptr<Scene> scene) {
            auto ent = lock.NewEntity();
            ent.Set<ecs::Owner>(lock, ecs::Owner::SystemId::INPUT_MANAGER);
            ent.Set<ecs::Name>(lock, consoleInputEntity.Name());
            ent.Set<ecs::SceneInfo>(lock, ent, scene);
            ent.Set<ecs::FocusLayer>(lock, ecs::FocusLayer::GAME);
            ent.Set<ecs::EventInput>(lock);
            auto &script = ent.Set<ecs::Script>(lock);
            script.AddOnTick([](ecs::Lock<ecs::WriteAll> lock, Tecs::Entity ent, double dtSinceLastFrame) {
                if (ent.Has<ecs::Script, ecs::EventInput>(lock)) {
                    auto &script = ent.Get<const ecs::Script>(lock);
                    auto &readInput = ent.Get<const ecs::EventInput>(lock);
                    bool hasEvents = false;
                    for (auto &queue : readInput.events) {
                        if (!queue.second.empty()) {
                            hasEvents = true;
                            break;
                        }
                    }
                    if (hasEvents) {
                        auto &input = ent.Get<ecs::EventInput>(lock);
                        for (auto &it : input.events) {
                            while (!it.second.empty()) {
                                auto command = script.GetParam<std::string>(it.first);
                                if (!command.empty()) { GetConsoleManager().QueueParseAndExecute(command); }
                                it.second.pop();
                            }
                        }
                    }
                }
            });
        });

        funcs.Register(this, "bind", "Bind a key to a command", &ConsoleBindingManager::BindKey);
    }

    void ConsoleBindingManager::SetConsoleInputCommand(
        ecs::Lock<ecs::Read<ecs::Name>, ecs::Write<ecs::Script, ecs::EventInput>> lock,
        std::string eventName,
        std::string command) {
        auto consoleInput = consoleInputEntity.Get(lock);
        if (!consoleInput.Has<ecs::Script, ecs::EventInput>(lock)) Abort("Console input entity is invalid");

        auto &eventInput = consoleInput.Get<ecs::EventInput>(lock);
        if (!eventInput.IsRegistered(eventName)) eventInput.Register(eventName);
        auto &script = consoleInput.Get<ecs::Script>(lock);
        script.SetParam(eventName, command);
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
                SetConsoleInputCommand(lock, eventName, command);

                auto &bindings = keyboard.Get<ecs::EventBindings>(lock);
                bindings.Unbind(eventName, consoleInputEntity, eventName);
                bindings.Bind(eventName, consoleInputEntity, eventName);
            }
        } else {
            Errorf("Key \"%s\" does not exist", keyName);
        }
    }
} // namespace sp
