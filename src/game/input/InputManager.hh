#pragma once

#include "ActionValue.hh"

#include <Common.hh>
#include <core/CFunc.hh>
#include <functional>
#include <game/gui/GuiManager.hh>
#include <glm/glm.hpp>
#include <list>
#include <mutex>
#include <robin_hood.h>
#include <set>
#include <shared_mutex>

namespace sp {
	/**
	 * Custom action types
	 */

	typedef robin_hood::unordered_flat_set<int> KeyEvents;
	typedef std::list<unsigned int> CharEvents;

	// Stores a simultanious mouse button press and position.
	struct ClickEvent {
		int button;
		glm::vec2 pos;
		bool down;

		ClickEvent(int button, glm::vec2 pos, bool down) : button(button), pos(pos), down(down) {}

		bool operator==(const ClickEvent &other) const {
			return button == other.button && pos == other.pos && down == other.down;
		}
	};
	typedef std::list<ClickEvent> ClickEvents;

	/**
	 * Generic Action paths
	 */
	static const std::string INPUT_ACTION_BASE = "/actions/main/in";
	static const std::string INPUT_ACTION_PRIMARY_TRIGGER = INPUT_ACTION_BASE + "/primary_trigger";
	static const std::string INPUT_ACTION_SECONDARY_TRIGGER = INPUT_ACTION_BASE + "/secondary_trigger";

	static const std::string INPUT_ACTION_PLAYER_BASE = "/actions/main/in/player";
	static const std::string INPUT_ACTION_TELEPORT = INPUT_ACTION_PLAYER_BASE + "/teleport"; // bool
	static const std::string INPUT_ACTION_GRAB = INPUT_ACTION_PLAYER_BASE + "/grab";		 // bool

	/**
	 * Glfw Action paths
	 */
	static const std::string INPUT_ACTION_KEYBOARD_BASE = "/actions/main/in/keyboard";
	static const std::string INPUT_ACTION_KEYBOARD_CHARS = INPUT_ACTION_KEYBOARD_BASE + "/chars"; // CharEvents
	static const std::string INPUT_ACTION_KEYBOARD_KEYS = INPUT_ACTION_KEYBOARD_BASE + "/keys";	  // KeyEvents

	static const std::string INPUT_ACTION_MOUSE_BASE = "/actions/main/in/mouse";
	static const std::string INPUT_ACTION_MOUSE_CLICK = INPUT_ACTION_MOUSE_BASE + "/click";				// ClickEvents
	static const std::string INPUT_ACTION_MOUSE_BUTTON_LEFT = INPUT_ACTION_MOUSE_BASE + "/button_left"; // bool
	static const std::string INPUT_ACTION_MOUSE_BUTTON_MIDDLE = INPUT_ACTION_MOUSE_BASE + "/button_middle"; // bool
	static const std::string INPUT_ACTION_MOUSE_BUTTON_RIGHT = INPUT_ACTION_MOUSE_BASE + "/button_right";	// bool
	static const std::string INPUT_ACTION_MOUSE_CURSOR = INPUT_ACTION_MOUSE_BASE + "/cursor";				// glm::vec2
	static const std::string INPUT_ACTION_MOUSE_SCROLL = INPUT_ACTION_MOUSE_BASE + "/scroll";				// glm::vec2

	class ActionSource;

	/**
	 * Class to manage input actions.  Call this->BindCallbacks() to set it up.
	 */
	class InputManager {
		friend ActionSource;

	public:
		InputManager();
		~InputManager();

		/**
		 * Returns true if the action exists, otherwise false.
		 * The action state will be returned through `value`.
		 */
		template<class T>
		bool GetActionValue(std::string actionPath, T **value);

		/**
		 * Returns true if the action exists, otherwise false.
		 * The change in action state since the last frame will be returned through `delta`.
		 */
		template<class T>
		bool GetActionDelta(std::string actionPath, T **value, T **delta);

		/**
		 * Returns true if the action exists and the state is true, otherwise false.
		 * This is an alias helper for the below GetActionStateValue() function.
		 */
		bool IsDown(std::string actionPath);

		/**
		 * Returns true if the action exists and the state changed from false to true this frame, otherwise false.
		 * This is an alias helper for the below GetActionStateDelta() function.
		 */
		bool IsPressed(std::string actionPath);

		/**
		 * Snapshots the current action states. These will be the
		 * values that are retrieved until the next frame.
		 */
		void BeginFrame();

		/**
		 * Returns true if input is currently consumed by a foreground system of higher input priority.
		 */
		bool FocusLocked(FocusLevel priority = FOCUS_GAME) const;

		/**
		 * Enables or disables the focus lock at a given priority.
		 * Returns false if the lock is already held.
		 */
		bool LockFocus(bool locked, FocusLevel priority = FOCUS_GAME);

		/**
		 * Bind an action to a console command
		 * Runs `command` when the state of a bool action changes.
		 */
		void BindCommand(string action, string command);

		/**
		 * Unbind an action from a console command.
		 */
		void UnbindCommand(string action);

	protected:
		void AddActionSource(ActionSource *source);
		void RemoveActionSource(ActionSource *source);

		template<class T>
		void SetAction(std::string actionPath, const T *value);

	private:
		// CFunc
		void BindKey(string args);

		vector<FocusLevel> focusLocked;

		CFuncCollection funcs;

		std::mutex commandsLock;
		robin_hood::unordered_flat_map<string, string> commandBindings;

		std::mutex sourcesLock;
		std::set<ActionSource *> sources;

		// The action states for the current, and previous frames.
		std::shared_mutex actionStatesLock;
		robin_hood::unordered_flat_map<string, std::shared_ptr<ActionValueBase>> actionStatesCurrent,
			actionStatesPrevious;
	};

	template<class T>
	inline bool InputManager::GetActionValue(std::string actionPath, T **value) {
		if (value == nullptr)
			return false;

		std::shared_lock lock(actionStatesLock);

		auto it = actionStatesCurrent.find(actionPath);
		if (it != actionStatesCurrent.end()) {
			*value = it->second->Get<T>();
			return *value != nullptr;
		}
		*value = nullptr;
		return false;
	}

	template<class T>
	inline bool InputManager::GetActionDelta(std::string actionPath, T **value, T **previous) {
		if (value == nullptr || previous == nullptr)
			return false;
		std::shared_lock lock(actionStatesLock);

		auto it = actionStatesCurrent.find(actionPath);
		if (it != actionStatesCurrent.end()) {
			*value = it->second->Get<T>();
		} else {
			*value = nullptr;
		}
		auto it2 = actionStatesPrevious.find(actionPath);
		if (it2 != actionStatesPrevious.end()) {
			*previous = it2->second->Get<T>();
		} else {
			*previous = nullptr;
		}

		return *value != nullptr;
	}

	template<class T>
	inline void InputManager::SetAction(std::string actionPath, const T *value) {
		std::lock_guard lock(actionStatesLock);
		if (value != nullptr) {
			auto ptrValue = std::shared_ptr<ActionValueBase>(new ActionValue<const T>(*value));
			auto it = actionStatesCurrent.find(actionPath);
			if (it != actionStatesCurrent.end()) {
				it->second = std::move(ptrValue);
			} else {
				actionStatesCurrent[actionPath] = std::move(ptrValue);
			}
		} else {
			actionStatesCurrent.erase(actionPath);
		}
	}
} // namespace sp
