#include "ConsoleBindingManager.hh"

#include "console/Console.hh"
#include "ecs/EcsImpl.hh"
#include "input/BindingNames.hh"
#include "input/KeyCodes.hh"

namespace sp {
    ConsoleBindingManager::ConsoleBindingManager() : consoleInputEntity("console-input"), keyboardEntity("keyboard") {
        {
            auto lock = ecs::World.StartTransaction<ecs::AddRemove>();

            auto ent = consoleInputEntity.Get(lock);
            if (!ent) {
                Logf("Creating console-input binding entity");
                ent = lock.NewEntity();
                ent.Set<ecs::Owner>(lock, ecs::Owner::SystemId::INPUT_MANAGER);
                ent.Set<ecs::Name>(lock, consoleInputEntity.Name());
                ent.Set<ecs::FocusLayer>(lock, ecs::FocusLayer::GAME);
                consoleInputEntity = ecs::NamedEntity(consoleInputEntity.Name(), ent);
            }
            ent.Get<ecs::EventInput>(lock);
            auto &script = ent.Get<ecs::Script>(lock);
            script.AddOnTick([](ecs::Lock<ecs::WriteAll> lock, Tecs::Entity ent, double dtSinceLastFrame) {
                if (ent.Has<ecs::Script, ecs::EventInput>(lock)) {
                    auto &script = ent.GetPrevious<ecs::Script>(lock);
                    auto &readInput = ent.GetPrevious<ecs::EventInput>(lock);
                    bool hasEvents = false;
                    for (auto &queue : readInput.events) {
                        if (!queue.second.empty()) {
                            hasEvents = true;
                            break;
                        }
                    }
                    if (hasEvents) {
                        auto &input = ent.Get<ecs::EventInput>(lock);
                        for (auto &[name, queue] : input.events) {
                            while (!queue.empty()) {
                                auto command = script.GetParam<std::string>(name);
                                if (!command.empty()) { GetConsoleManager().QueueParseAndExecute(command); }
                                queue.pop();
                            }
                        }
                    }
                }
            });
        }

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
