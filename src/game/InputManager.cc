#include "InputManager.hh"
#include "Common.hh"

#include <unordered_map>
#include <algorithm>
#include <stdexcept>
#include <GLFW/glfw3.h>

namespace sp
{
	InputManager::InputManager()
	{
		keysDown.fill(false);
		keysPressed.fill(false);
		keysReleased.fill(false);
		firstCursorAction = true;
	}

	InputManager::~InputManager() {}

	bool InputManager::IsDown(int key) const
	{
		return checkpointKeysDown.at(key);
	}

	bool InputManager::IsPressed(int key) const
	{
		return checkpointKeysPressed.at(key);
	}

	bool InputManager::IsReleased(int key) const
	{
		return checkpointKeysReleased.at(key);
	}

	bool InputManager::IsAnyDown(vector<int> keys) const
	{
		return std::any_of(keys.begin(), keys.end(), [this](int key)
		{
			return IsDown(key);
		});
	}

	bool InputManager::IsAnyPressed(vector<int> keys) const
	{
		return std::any_of(keys.begin(), keys.end(), [this](int key)
		{
			return IsPressed(key);
		});
	}

	bool InputManager::IsAnyReleased(vector<int> keys) const
	{
		return std::any_of(keys.begin(), keys.end(), [this](int key)
		{
			return IsReleased(key);
		});
	}

	/**
	 * Saves the current cursor as the one to compute the CursorDiff from.
	 */
	void InputManager::Checkpoint()
	{
		// initialize previous cursor value to the current one first time around
		if (firstCursorAction)
		{
			firstCursorAction = false;
			Checkpoint();
		}

		checkpointCursorDiff = cursor - checkpointCursor;
		checkpointCursor = cursor;

		checkpointScrollOffset = scrollOffset;
		scrollOffset = glm::vec2(0, 0);

		// checkpoint the key states
		checkpointKeysPressed = keysPressed;
		checkpointKeysReleased = keysReleased;
		checkpointKeysDown = keysDown;

		// reset the pressed/unpressed states since these do not normally reset
		keysPressed.fill(false);
		keysReleased.fill(false);
	}

	glm::vec2 InputManager::CursorDiff() const
	{
		return checkpointCursorDiff;
	}

	glm::vec2 InputManager::Cursor() const
	{
		return checkpointCursor;
	}

	glm::vec2 InputManager::ImmediateCursor() const
	{
		if (glfwGetWindowAttrib(window, GLFW_FOCUSED))
		{
			double mouseX, mouseY;
			glfwGetCursorPos(window, &mouseX, &mouseY);
			return glm::vec2((float) mouseX, (float) mouseY);
		}
		else
		{
			return glm::vec2(-1.0f, -1.0f);
		}
	}

	glm::vec2 InputManager::ScrollOffset() const
	{
		return checkpointScrollOffset;
	}

	void InputManager::SetCursorPosition(glm::vec2 pos)
	{
		glfwSetCursorPos(window, pos.x, pos.y);
	}

	bool InputManager::FocusLocked() const
	{
		return focusLocked;
	}

	bool InputManager::LockFocus(bool locked)
	{
		if (locked && focusLocked)
			return false;

		focusLocked = locked;
		return true;
	}

	void InputManager::KeyInputCallback(
		GLFWwindow *window,
		int key,
		int scancode,
		int action,
		int mods)
	{
		auto im = static_cast<InputManager *>(glfwGetWindowUserPointer(window));
		im->keyChange(key, action);
	}

	void InputManager::CharInputCallback(
		GLFWwindow *window,
		unsigned int ch)
	{
		auto im = static_cast<InputManager *>(glfwGetWindowUserPointer(window));
		im->charInput(ch);
	}

	void InputManager::keyChange(int key, int action)
	{
		switch (action)
		{
			case GLFW_PRESS:
				keysDown[key] = true;
				keysPressed[key] = true;
				break;
			case GLFW_RELEASE:
				keysDown[key] = false;
				keysReleased[key] = true;
				break;
		}

		for (auto &cb : keyEventCallbacks)
		{
			cb(key, action);
		}
	}

	void InputManager::charInput(uint32 ch)
	{
		for (auto &cb : charEventCallbacks)
		{
			cb(ch);
		}
	}

	void InputManager::MouseMoveCallback(
		GLFWwindow *window,
		double xPos,
		double yPos)
	{
		auto im = static_cast<InputManager *>(glfwGetWindowUserPointer(window));

		im->cursor.x = (float) xPos;
		im->cursor.y = (float) yPos;
	}

	void InputManager::MouseButtonCallback(
		GLFWwindow *window,
		int button,
		int action,
		int mods)
	{
		auto im = static_cast<InputManager *>(glfwGetWindowUserPointer(window));

		int key = MouseButtonToKey(button);
		im->keyChange(key, action);
	}

	void InputManager::MouseScrollCallback(GLFWwindow *window, double xOffset, double yOffset)
	{
		auto im = static_cast<InputManager *>(glfwGetWindowUserPointer(window));

		im->scrollOffset.x += (float) xOffset;
		im->scrollOffset.y += (float) yOffset;
	}

	void InputManager::BindCallbacks(GLFWwindow *window)
	{
		this->window = window;

		glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

		// store a pointer to this InputManager since we must provide static functions
		glfwSetWindowUserPointer(window, this);

		glfwSetKeyCallback(window, KeyInputCallback);
		glfwSetCharCallback(window, CharInputCallback);
		glfwSetScrollCallback(window, MouseScrollCallback);
		glfwSetMouseButtonCallback(window, MouseButtonCallback);
		glfwSetCursorPosCallback(window, MouseMoveCallback);
	}

	int MouseButtonToKey(int button)
	{
		if (button > GLFW_MOUSE_BUTTON_LAST)
		{
			throw std::runtime_error("invalid mouse button");
		}
		return GLFW_KEY_LAST + 1 + button;
	}
}