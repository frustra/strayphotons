#pragma once

#include "InputManager.hh"

#include <robin_hood.h>
#include <string>

namespace sp {
    template<class T>
    class ActionValue;

    /**
     * An action source is a system that converts device input into actions.
     * Specific implementations register themselves with the InputManager, which aggregates all sources.
     */
    class ActionSource {
    public:
        ActionSource(InputManager &inputManager);
        ~ActionSource();

        /**
         * Save any action value changes to the InputManager
         */
        virtual void BeginFrame() = 0;

        /**
         * Bind an action to another action.
         * `action` will follow the state of `source`.
         */
        virtual void BindAction(std::string action, std::string source);

        /**
         * Unbind an action from a source.
         */
        virtual void UnbindAction(std::string action);

    protected:
        template<class T>
        void SetAction(std::string actionPath, const T &value);

    private:
        InputManager *input;
        robin_hood::unordered_flat_map<std::string, robin_hood::unordered_flat_set<std::string>> actionBindings;

        friend class InputManager;
    };

    template<class T>
    inline void ActionSource::SetAction(std::string actionPath, const T &value) {
        if (input != nullptr) {
            input->SetAction(actionPath, value);

            // Set any alias if it exists.
            auto it = actionBindings.find(actionPath);
            if (it != actionBindings.end()) {
                for (auto action : it->second) {
                    input->SetAction(action, value);
                }
            }
        }
    }
} // namespace sp
