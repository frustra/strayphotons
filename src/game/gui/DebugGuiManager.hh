#pragma once

#include "game/GuiManager.hh"

namespace sp
{
	class InputManager;

	class DebugGuiManager : public GuiManager
	{
	public:
		DebugGuiManager() { }
		virtual ~DebugGuiManager() { }

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

		const int FocusLevel = 1000;

	private:
		InputManager *inputManager = nullptr;

		bool consoleOpen = false;
		glm::vec2 guiCursorPos = { 200.0f, 200.0f };
	};
}
