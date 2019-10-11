#pragma once

#include "game/GuiManager.hh"

namespace sp
{
	class InputManager;

	class DebugGuiManager : public GuiManager
	{
	public:
		DebugGuiManager() : GuiManager(FOCUS_OVERLAY) { }
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

	private:
		InputManager *inputManager = nullptr;

		bool consoleOpen = false;
		glm::vec2 guiCursorPos = { 200.0f, 200.0f };
	};
}
