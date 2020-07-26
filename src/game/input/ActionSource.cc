
#include "ActionSource.hh"

#include "ActionValue.hh"
#include "InputManager.hh"

#include <Common.hh>

namespace sp {
	ActionSource::ActionSource(InputManager &inputManager) : input(&inputManager) {
		inputManager.AddActionSource(this);
	}

	ActionSource::~ActionSource() {
		if (input != nullptr) {
			input->RemoveActionSource(this);
		}
	}

	void ActionSource::BindAction(std::string action, std::string alias) {
		actionBindings[action] = alias;
	}

	void ActionSource::UnbindAction(std::string action) {
		actionBindings.erase(action);
	}
} // namespace sp
