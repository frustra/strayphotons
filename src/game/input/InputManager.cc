#include "InputManager.hh"

#include "ActionSource.hh"
#include "BindingNames.hh"

#include <Common.hh>
#include <algorithm>
#include <core/Console.hh>
#include <core/Logging.hh>
#include <stdexcept>

namespace sp {
    InputManager::InputManager() {
        funcs.Register(this, "bind", "Bind a key to a command", &InputManager::BindKey);
    }

    InputManager::~InputManager() {
        for (auto source : sources) {
            if (source != nullptr) { source->input = nullptr; }
        }
    }

    bool InputManager::IsDown(std::string actionPath) {
        const bool *value;
        if (GetActionValue(actionPath, &value)) return *value;
        return false;
    }

    /**
     * Returns true if the action exists and the state changed from false to true this frame, otherwise false.
     * This is an alias helper for the below GetActionStateDelta() function.
     */
    bool InputManager::IsPressed(std::string actionPath) {
        const bool *value, *previous;
        if (GetActionDelta(actionPath, &value, &previous)) {
            if (previous != nullptr) { return !*previous && *value; }
            return *value;
        }
        return false;
    }

    void InputManager::BeginFrame() {
        { // Advance input action snapshots 1 frame.
            std::lock_guard lock(actionStatesLock);
            actionStatesPrevious = actionStatesCurrent;
        }

        {
            std::lock_guard lock(sourcesLock);
            for (auto source : sources) {
                source->BeginFrame();
            }
        }

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
    }

    bool InputManager::FocusLocked(FocusLevel priority) const {
        return focusLocked.size() > 0 && focusLocked.back() > priority;
    }

    bool InputManager::LockFocus(bool locked, FocusLevel priority) {
        if (locked) {
            if (focusLocked.empty() || focusLocked.back() < priority) {
                focusLocked.push_back(priority);
                return true;
            }
        } else {
            if (!focusLocked.empty() && focusLocked.back() == priority) {
                focusLocked.pop_back();
                return true;
            }
        }

        return false;
    }

    void InputManager::BindCommand(string action, string command) {
        std::lock_guard lock(commandsLock);
        commandBindings[action] = command;
    }

    void InputManager::UnbindCommand(string action) {
        std::lock_guard lock(commandsLock);
        commandBindings.erase(action);
    }

    void InputManager::AddActionSource(ActionSource *source) {
        std::lock_guard lock(sourcesLock);
        sources.insert(source);
    }

    void InputManager::RemoveActionSource(ActionSource *source) {
        std::lock_guard lock(sourcesLock);
        sources.erase(source);
    }

    void InputManager::UnsetAction(std::string actionPath) {
        std::lock_guard lock(actionStatesLock);
        actionStatesCurrent.erase(actionPath);
    }

    // TODO: Fix to create a user-defined action
    void InputManager::BindKey(string args) {
        std::stringstream stream(args);
        string keyName;
        stream >> keyName;

        auto it = UserBindingNames.find(to_upper_copy(keyName));
        if (it != UserBindingNames.end()) {
            string command;
            std::getline(stream, command);
            trim(command);

            Logf("Binding %s to command: %s", keyName, command);
            BindCommand(it->second, command);
        } else {
            Errorf("Binding %s does not exist", keyName);
        }
    }
} // namespace sp
