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
		for (uint32 i = 0; i < InputManager::MAX_KEYS; ++i)
		{
			this->keysPressed[i] = false;
		}

		this->firstCursorAction = true;
	}

	InputManager::~InputManager(){}

	bool InputManager::IsDown(int key) const
	{
		return this->checkpointKeysDown.at(key);
	}

	bool InputManager::IsPressed(int key) const
	{
		return this->checkpointKeysPressed.at(key);
	}

	bool InputManager::IsReleased(int key) const
	{
		return this->checkpointKeysReleased.at(key);
	}

	bool InputManager::IsAnyDown(vector<int> keys) const
	{
		return std::any_of(keys.begin(), keys.end(), [this](int key){
			return IsDown(key);
		});
	}

	bool InputManager::IsAnyPressed(vector<int> keys) const
	{
		return std::any_of(keys.begin(), keys.end(), [this](int key){
			return IsPressed(key);
		});
	}

	bool InputManager::IsAnyReleased(vector<int> keys) const
	{
		return std::any_of(keys.begin(), keys.end(), [this](int key){
			return IsReleased(key);
		});
	}

	/**
	 * Saves the current cursor as the one to compute the CursorDiff from.
	 */
	void InputManager::Checkpoint()
	{
		// initialize previous cursor value to the current one first time around
		if (this->firstCursorAction)
		{
			this->firstCursorAction = false;
			Checkpoint();
		}

		this->checkpointCursorDiff = this->cursor - this->checkpointCursor;
		this->checkpointCursor = this->cursor;

		this->checkpointScrollOffset = this->scrollOffset;
		this->scrollOffset = glm::vec2(0, 0);

		// checkpoint the key states
		this->checkpointKeysPressed = this->keysPressed;
		this->checkpointKeysReleased = this->keysReleased;
		this->checkpointKeysDown = this->keysDown;

		// reset the pressed/unpressed states since these do not normally reset
		this->keysPressed.fill(false);
		this->keysReleased.fill(false);
	}

	glm::vec2 InputManager::CursorDiff() const
	{
		return this->checkpointCursorDiff;
	}

	glm::vec2 InputManager::Cursor() const
	{
		return this->checkpointCursor;
	}

	glm::vec2 InputManager::ScrollOffset() const
	{
		return this->checkpointScrollOffset;
	}

	void InputManager::KeyInputCallback(
		GLFWwindow * window,
		int key,
		int scancode,
		int action,
		int mods)
	{
		auto im = static_cast<InputManager*>(glfwGetWindowUserPointer(window));
		im->keyChange(key, action);
	}

	void InputManager::keyChange(int key, int action)
	{
		switch (action) {
		case GLFW_PRESS:
			this->keysDown[key] = true;
			this->keysPressed[key] = true;
			break;
		case GLFW_RELEASE:
			this->keysDown[key] = false;
			this->keysReleased[key] = true;
			break;
		}
	}

	void InputManager::MouseMoveCallback(
		GLFWwindow * window,
		double xPos,
		double yPos)
	{
		auto im = static_cast<InputManager*>(glfwGetWindowUserPointer(window));

		im->cursor.x = xPos;
		im->cursor.y = yPos;
	}

	void InputManager::MouseButtonCallback(
		GLFWwindow * window,
		int button,
		int action,
		int mods)
	{
		auto im = static_cast<InputManager*>(glfwGetWindowUserPointer(window));

		int key = MouseButtonToKey(button);
		im->keyChange(key, action);
	}

	void InputManager::MouseScrollCallback(GLFWwindow * window, double xOffset, double yOffset)
	{
		auto im = static_cast<InputManager*>(glfwGetWindowUserPointer(window));

		im->scrollOffset.x += xOffset;
		im->scrollOffset.y += yOffset;
	}

	void InputManager::BindCallbacks(GLFWwindow * window)
	{
		// store a pointer to this InputManager since we must provide static functions
		glfwSetWindowUserPointer(window, this);

		glfwSetKeyCallback(window, KeyInputCallback);
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