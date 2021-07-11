
#include "ActionSource.hh"

#include "ActionValue.hh"
#include "InputManager.hh"
#include "core/Common.hh"

namespace sp {
    ActionSource::ActionSource(InputManager &inputManager) : input(&inputManager) {
        inputManager.AddActionSource(this);
    }

    ActionSource::~ActionSource() {
        if (input != nullptr) { input->RemoveActionSource(this); }
    }

    void ActionSource::BindAction(std::string action, std::string source) {
        auto it = actionBindings.find(source);
        if (it != actionBindings.end()) {
            actionBindings[source].emplace(action);
        } else {
            actionBindings.emplace(source, std::initializer_list<std::string>({action}));
        }
    }

    void ActionSource::UnbindAction(std::string action) {
        for (auto &[source, actions] : actionBindings) {
            actions.erase(action);
        }
    }
} // namespace sp
