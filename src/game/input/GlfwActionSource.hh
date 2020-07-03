#pragma once

#include "ActionSource.hh"
#include "InputManager.hh"

#include <robin_hood.h>
#include <glm/glm.hpp>
#include <queue>
#include <mutex>

struct GLFWwindow;

namespace sp
{
	/**
	 * Glfw input source. Provides mouse and keyboard actions.
	 */
	class GlfwActionSource : public ActionSource
	{
	public:
		GlfwActionSource(InputManager &inputManager, GLFWwindow &window);
		~GlfwActionSource() {}

		/**
		 * Saves the current cursor, scroll, and key values. These will be the
		 * values that are retrieved until the next frame.
		 */
		void BeginFrame() override;

		/**
		 * Returns the x,y position of the current cursor, even if it has moved since the start of frame.
		 */
		glm::vec2 ImmediateCursor() const;

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

	private:
		GLFWwindow *window = nullptr;

		std::mutex dataLock;
		glm::vec2 mousePos, mouseScroll;
		CharEvents charEvents, charEventsNext;
		KeyEvents keyEvents, keyEventsNext;
		ClickEvents clickEvents, clickEventsNext;
	};
}
