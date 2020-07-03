#pragma once

#include <Common.hh>
#include <array>
#include <core/CFunc.hh>
#include <functional>
#include <game/gui/GuiManager.hh>
#include <glm/glm.hpp>
#include <robin_hood.h>

namespace sp {
	static const std::string INPUT_ACTION_BASE = "/actions/main/in";
	static const std::string INPUT_ACTION_PRIMARY_TRIGGER = INPUT_ACTION_BASE + "/primary_trigger";
	static const std::string INPUT_ACTION_SECONDARY_TRIGGER = INPUT_ACTION_BASE + "/secondary_trigger";

	static const std::string INPUT_ACTION_PLAYER_BASE = "/actions/main/in/player";
	static const std::string INPUT_ACTION_TELEPORT = INPUT_ACTION_PLAYER_BASE + "/teleport";
	static const std::string INPUT_ACTION_GRAB = INPUT_ACTION_PLAYER_BASE + "/grab";

	static const std::string INPUT_ACTION_KEYBOARD_BASE = "/actions/main/in/keyboard";
	static const std::string INPUT_ACTION_MOUSE_BASE = "/actions/main/in/mouse";
	static const std::string INPUT_ACTION_MOUSE_CURSOR = INPUT_ACTION_BASE + "/cursor";
	static const std::string INPUT_ACTION_MOUSE_SCROLL = INPUT_ACTION_BASE + "/scroll";

	/**
	 * Class to manage hardware input.  Call this->BindCallbacks() to set it up.
	 */
	class InputManager {
	public:
		InputManager();
		~InputManager();

		/**
		 * Returns true if the action exists and the state is true, otherwise false.
		 * This is an alias helper for the below GetActionStateValue() function.
		 */
		inline bool IsDown(std::string actionPath) const {
			bool value = false;
			if (GetActionStateValue(actionPath, value))
				return value;
			return false;
		}

		/**
		 * Returns true if the action exists and the state changed from false to true this frame, otherwise false.
		 * This is an alias helper for the below GetActionStateDelta() function.
		 */
		inline bool IsPressed(std::string actionPath) const {
			bool value = false;
			bool changed = false;
			if (GetActionStateDelta(actionPath, value, changed))
				return value && changed;
			return false;
		}

		/**
		 * Returns true if the action exists, otherwise false.
		 * The action state will be returned through `value`.
		 */
		virtual bool GetActionStateValue(std::string actionPath, bool &value) const {
			return false;
		}
		virtual bool GetActionStateValue(std::string actionPath, float &value) const {
			return false;
		}
		virtual bool GetActionStateValue(std::string actionPath, glm::vec2 &value) const {
			return false;
		}

		/**
		 * Returns true if the action exists, otherwise false.
		 * The change in action state since the last frame will be returned through `delta`.
		 */
		virtual bool GetActionStateDelta(std::string actionPath, bool &value, bool &delta) const {
			return false;
		}
		virtual bool GetActionStateDelta(std::string actionPath, float &value, float &delta) const {
			return false;
		}
		virtual bool GetActionStateDelta(std::string actionPath, glm::vec2 &value, glm::vec2 &delta) const {
			return false;
		}

		/**
		 * Snapshots the current action states. These will be the
		 * values that are retrieved until the next frame.
		 */
		virtual void BeginFrame() {}

		/**
		 * Returns the x,y position of the current cursor, even if it has moved since the start of frame.
		 */
		virtual glm::vec2 ImmediateCursor() const {
			return glm::vec2(-1.0f, -1.0f);
		}

		/**
		 * Hide the cursor from being displayed.
		 */
		virtual void DisableCursor(){};

		/**
		 * Restore the cursor being displayed.
		 */
		virtual void EnableCursor(){};

		/**
		 * Returns true if input is currently consumed by a foreground system of higher input priority.
		 */
		bool FocusLocked(FocusLevel priority = FOCUS_GAME) const;

		/**
		 * Enables or disables the focus lock at a given priority.
		 * Returns false if the lock is already held.
		 */
		bool LockFocus(bool locked, FocusLevel priority = FOCUS_GAME);

		typedef std::function<void(uint32)> CharEventCallback;

		/**
		 * Register a function to be called when an input character is received.
		 */
		void AddCharInputCallback(CharEventCallback cb) {
			charEventCallbacks.push_back(cb);
		}

		/**
		 * Bind an action to a console command
		 * Runs `command` when the state of a bool action changes.
		 */
		void BindCommand(string action, string command) {
			commandBindings[action] = command;
		}

		/**
		 * Unbind an action from a console command.
		 */
		void UnbindCommand(string action) {
			commandBindings.erase(action);
		}

		/**
		 * Bind an action to another action.
		 * `alias` will follow the state of `action`.
		 */
		void BindAction(string action, string alias) {
			actionBindings[alias] = action;
		}

		/**
		 * Unbind an action from another action.
		 */
		void UnbindAction(string alias) {
			actionBindings.erase(alias);
		}

		// CFunc
		void BindKey(string args);

	protected:
		vector<CharEventCallback> charEventCallbacks;
		vector<FocusLevel> focusLocked;

		CFuncCollection funcs;
		robin_hood::unordered_flat_map<string, string> commandBindings;
		robin_hood::unordered_flat_map<string, string> actionBindings;
	};
} // namespace sp
