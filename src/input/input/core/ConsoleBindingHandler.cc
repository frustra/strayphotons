#include "ConsoleBindingHandler.hh"

#include "core/Common.hh"
#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"
#include "input/core/BindingNames.hh"
#include "input/core/KeyCodes.hh"

#include <sstream>

namespace sp {
    ConsoleBindingHandler::ConsoleBindingHandler() : keyboardEntity("keyboard") {
        funcs.Register(this, "bind", "Bind a key to a command", &ConsoleBindingHandler::BindKey);

        {
            auto lock = ecs::World.StartTransaction<ecs::AddRemove>();

            bindingEntity = lock.NewEntity();
            bindingEntity.Set<ecs::EventBindings>(lock);
        }
    }

    /*void ConsoleBindingHandler::Frame() {
        if (!FocusLocked()) {
            // Run any command bindings
            std::lock_guard lock(commandsLock);
            for (auto [actionPath, command] : commandBindings) {
                if (IsPressed(actionPath)) {
                    // Run the bound command
                    GetConsoleManager().QueueParseAndExecute(command);
                }
            }
        }
    }*/

    void ConsoleBindingHandler::BindKey(string args) {
        std::stringstream stream(args);
        std::string keyName;
        stream >> keyName;

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
            string command;
            std::getline(stream, command);
            trim(command);

            {
                auto lock = ecs::World.StartTransaction<ecs::Read<ecs::Name>, ecs::Write<ecs::EventBindings>>();

                auto keyboard = keyboardEntity.Get(lock);
                if (keyboard.Has<ecs::EventBindings>(lock)) {
                    auto &bindings = keyboard.Get<ecs::EventBindings>(lock);

                    Logf("Binding %s to command: %s", keyName, command);
                    // TODO: Fix command binding
                    // bindings.BindCommand(it->second, command);
                } else {
                    Errorf("Can't bind key without valid keyboard entity");
                }
            }
        } else {
            Errorf("Binding \"%s\" does not exist", keyName);
        }
    }
} // namespace sp
