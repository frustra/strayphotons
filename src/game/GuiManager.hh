#pragma once

#include <glm/glm.hpp>

namespace sp
{
	class InputManager;

	class GuiManager
	{
	public:
		GuiManager();
		void BeforeFrame();
		void DefineWindows();

		bool Focused()
		{
			return consoleOpen;
		}

		void BindInput(InputManager &inputManager);
		void GrabFocus();
		void ReleaseFocus();

		void ToggleConsole();

	private:
		InputManager *inputManager = nullptr;

		bool consoleOpen = false;
		glm::vec2 guiCursorPos = { 200.0f, 200.0f };
	};
}