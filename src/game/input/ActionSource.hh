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
		 * `alias` will follow the state of `action`.
		 */
		virtual void BindAction(std::string action, std::string alias);

		/**
		 * Unbind an action from another action.
		 */
		virtual void UnbindAction(std::string action);

	protected:
		template<class T>
		void SetAction(std::string actionPath, const T &value);

	private:
		InputManager *input;
		robin_hood::unordered_flat_map<std::string, std::string> actionBindings;
	};

	template<class T>
	inline void ActionSource::SetAction(std::string actionPath, const T &value) {
		if (input != nullptr) {
			input->SetAction(actionPath, value);

			// Set any alias if it exists.
			auto it = actionBindings.find(actionPath);
			if (it != actionBindings.end()) {
				input->SetAction(it->second, value);
			}
		}
	}
} // namespace sp
