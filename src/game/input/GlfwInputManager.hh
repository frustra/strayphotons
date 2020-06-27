#pragma once

#include <glm/glm.hpp>
#include <array>
#include <robin_hood.h>
#include <functional>
#include "InputManager.hh"

struct GLFWwindow;

namespace sp
{
	/**
	 * Class to manage hardware input.  Call this->BindCallbacks() to set it up.
	 */
	class GlfwInputManager : public InputManager
	{
	public:
		GlfwInputManager() {}
		~GlfwInputManager() {}

		/**
		 * Returns true if the action exists, otherwise false.
		 * The action state will be returned through `value`.
		 */
		virtual bool GetActionStateValue(std::string actionPath, bool &value) const;
		virtual bool GetActionStateValue(std::string actionPath, glm::vec2 &value) const;

		/**
		 * Returns true if the action exists, otherwise false.
		 * The change in action state since the last frame will be returned through `delta`.
		 */
		virtual bool GetActionStateDelta(std::string actionPath, bool &value, bool &delta) const;
		virtual bool GetActionStateDelta(std::string actionPath, glm::vec2 &value, glm::vec2 &delta) const;

		/**
		 * Saves the current cursor, scroll, and key values. These will be the
		 * values that are retrieved until the next frame.
		 */
		virtual void BeginFrame();

		/**
		 * Returns the x,y position of the current cursor, even if it has moved since the start of frame.
		 */
		virtual glm::vec2 ImmediateCursor() const;

		virtual void DisableCursor();
		virtual void EnableCursor();

		static void KeyInputCallback(
			GLFWwindow *window,
			int key,
			int scancode,
			int action,
			int mods
		);

		static void CharInputCallback(
			GLFWwindow *window,
			unsigned int ch
		);

		static void MouseMoveCallback(
			GLFWwindow *window,
			double xPos,
			double yPos
		);

		static void MouseButtonCallback(
			GLFWwindow *window,
			int button,
			int actions,
			int mods
		);

		static void MouseScrollCallback(GLFWwindow *window, double xOffset, double yOffset);

		/**
		 * Bind input callbacks from the given window to this object's callback functions
		 */
		void BindCallbacks(GLFWwindow *window);

		typedef std::function<void(int, int)> KeyEventCallback;

		/**
		 * Register a function to be called when an input character is received.
		 */
		void AddKeyInputCallback(KeyEventCallback cb)
		{
			keyEventCallbacks.push_back(cb);
		}

	private:
		vector<KeyEventCallback> keyEventCallbacks;
		GLFWwindow *window = nullptr;

        // The action states for the next, current, and previous frames.
		robin_hood::unordered_flat_map<string, bool> actionStatesBoolNext, actionStatesBoolCurrent, actionStatesBoolPrevious;
		robin_hood::unordered_flat_map<string, glm::vec2> actionStatesVec2Next, actionStatesVec2Current, actionStatesVec2Previous;
	};
}
