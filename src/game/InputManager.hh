#pragma once

#include <glm/glm.hpp>
#include <GLFW/glfw3.h>
#include <array>
#include <functional>
#include "Common.hh"

namespace sp
{
	/**
	 * Class to manage hardware input.  Call this->BindCallbacks() to set it up.
	 */
	class InputManager
	{
	public:
		InputManager();
		~InputManager();

		/**
		 * Return true if the key is currently "down" otherwise false because it is "up".
		 */
		bool IsDown(int key) const;

		/**
		 * Return true if IsDown() returns true for any of the given keys.
		 */
		bool IsAnyDown(vector<int> keys) const;

		/**
		 * Return true if the key went from being "up" to "down" within the previous
		 *     two checkpoints.
		 */
		bool IsPressed(int key) const;

		/**
		 * Returns true if IsPressed() returns true for any of the given keys
		 */
		bool IsAnyPressed(vector<int> keys) const;

		/**
		 * Return true if the key went from being "down" to "up" within the previous
		 *     two checkpoints.
		 */
		bool IsReleased(int key) const;

		/**
		 * Returns true if IsReleased() returns true for any of the given keys
		 */
		bool IsAnyReleased(vector<int> keys) const;

		/**
		 * Saves the current cursor, scroll, and key values.  These will be the
		 * values that are retrieved until the next time Checkpoint() is called.
		 * This should be done at the beginning of a frame.
		 */
		void Checkpoint();

		/**
		 * Returns the difference between the previous and most recent checkpointed cursors'
		 *     xy values.
		 * First return element is x diff, 2nd is y diff.
		 */
		glm::vec2 CursorDiff() const;

		/**
		 * Returns the x,y position of the last checkpointed cursor.
		 */
		glm::vec2 Cursor() const;

		/**
		 * Returns the x,y position of the current cursor.
		 */
		glm::vec2 ImmediateCursor() const;

		/**
		 * Returns the difference between the previous and most recent checkpointed scroll
		 *     xy values.
		 */
		glm::vec2 ScrollOffset() const;

		/**
		 * Sets the virtual cursor position, relative to top left.
		 */
		void SetCursorPosition(glm::vec2 pos);

		/**
		 * Returns true if input is currently consumed by a foreground system.
		 */
		bool FocusLocked(int priority = 1) const;

		/**
		 * Enables or disables the focus lock at a given priority.
		 * Returns false if the lock is already held.
		 */
		bool LockFocus(bool locked, int priority = 1);

		void DisableCursor();
		void EnableCursor();

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
		typedef std::function<void(uint32)> CharEventCallback;

		/**
		 * Register a function to be called when a key state changes.
		 */
		void AddKeyInputCallback(KeyEventCallback cb)
		{
			keyEventCallbacks.push_back(cb);
		}

		/**
		 * Register a function to be called when an input character is received.
		 */
		void AddCharInputCallback(CharEventCallback cb)
		{
			charEventCallbacks.push_back(cb);
		}

	private:
		void keyChange(int key, int action);
		void charInput(uint32 ch);

	private:
		static const uint32 MAX_KEYS = GLFW_KEY_LAST + 1 + GLFW_MOUSE_BUTTON_LAST;

		GLFWwindow *window = nullptr;

		// keys that received a "press" event
		std::array<bool, MAX_KEYS> keysPressed;
		std::array<bool, MAX_KEYS> checkpointKeysPressed;

		// keys that received a "release" event
		std::array<bool, MAX_KEYS> keysReleased;
		std::array<bool, MAX_KEYS> checkpointKeysReleased;

		// keys in the "down" state
		std::array<bool, MAX_KEYS> keysDown;
		std::array<bool, MAX_KEYS> checkpointKeysDown;

		bool firstCursorAction;

		glm::vec2 cursor;
		glm::vec2 checkpointCursor;
		glm::vec2 checkpointCursorDiff;

		glm::vec2 scrollOffset;
		glm::vec2 checkpointScrollOffset;

		vector<KeyEventCallback> keyEventCallbacks;
		vector<CharEventCallback> charEventCallbacks;

		vector<int> focusLocked;
	};

	int MouseButtonToKey(int button);
}
